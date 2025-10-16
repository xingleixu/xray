/*
** xvm.c
** Xray 寄存器虚拟机实现
*/

#include "xvm.h"
#include "xdebug.h"
#include "xmem.h"
#include "xstring.h"
#include "xarray.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

/* 全局VM实例 */
VM vm;

/* ========== 分支预测优化宏 ========== */

/*
** likely/unlikely宏：帮助编译器进行分支预测优化
** GCC和Clang支持__builtin_expect内置函数
*/
#if defined(__GNUC__) || defined(__clang__)
  #define likely(x)    __builtin_expect(!!(x), 1)
  #define unlikely(x)  __builtin_expect(!!(x), 0)
#else
  #define likely(x)    (x)
  #define unlikely(x)  (x)
#endif

/* ========== Computed Goto优化（v0.13.6） ========== */

/*
** Computed Goto开关
** GCC和Clang支持computed goto（标签作为值）
** 这是最有效的VM dispatch优化技术，可提升15-30%性能
*/
#ifndef XR_USE_COMPUTED_GOTO
  #if defined(__GNUC__) || defined(__clang__)
    #define XR_USE_COMPUTED_GOTO 1
  #else
    #define XR_USE_COMPUTED_GOTO 0
  #endif
#endif

#if XR_USE_COMPUTED_GOTO
  /* Computed Goto模式：直接跳转到下一条指令 */
  #define DISPATCH() do { \
      inst = READ_INSTRUCTION(); \
      goto *dispatch_table[GET_OPCODE(inst)]; \
  } while(0)
  #define CASE(op) L_##op
  #define SWITCH_START() goto *dispatch_table[GET_OPCODE(inst)]
  #define SWITCH_END()
#else
  /* 传统Switch模式 */
  #define DISPATCH() break
  #define CASE(op) case op
  #define SWITCH_START() for(;;) { OpCode op = GET_OPCODE(inst); switch(op) {
  #define SWITCH_END() \
          default: \
              xr_bc_runtime_error("Unknown opcode %d", op); \
              return INTERPRET_RUNTIME_ERROR; \
      } inst = READ_INSTRUCTION(); }
#endif

/* ========== 辅助宏 ========== */

/* 读取下一条指令 */
#define READ_INSTRUCTION() (*frame->pc++)

/* 寄存器访问 */
#define R(i) (frame->base[i])
#define RA(inst) (frame->base + GETARG_A(inst))
#define RB(inst) (frame->base + GETARG_B(inst))
#define RC(inst) (frame->base + GETARG_C(inst))

/* 常量访问 */
#define K(i) (frame->closure->proto->constants.values[i])
#define KB(inst) (K(GETARG_B(inst)))
#define KC(inst) (K(GETARG_C(inst)))

/* ========== 运行时错误处理 ========== */

/*
** 报告运行时错误
*/
void xr_bc_runtime_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);
    
    /* 打印调用栈 */
    for (int i = vm.frame_count - 1; i >= 0; i--) {
        BcCallFrame *frame = &vm.frames[i];
        Proto *proto = frame->closure->proto;
        
        /* 计算指令偏移 */
        size_t instruction = frame->pc - proto->code - 1;
        
        fprintf(stderr, "[line ");
        if (instruction < proto->size_lineinfo && proto->lineinfo != NULL) {
            fprintf(stderr, "%d", proto->lineinfo[instruction]);
        } else {
            fprintf(stderr, "?");
        }
        fprintf(stderr, "] in ");
        
        if (proto->name != NULL) {
            fprintf(stderr, "%s()\n", proto->name->chars);
        } else {
            fprintf(stderr, "script\n");
        }
    }
    
    /* 重置栈 */
    vm.stack_top = vm.stack;
    vm.frame_count = 0;
}

/* ========== Upvalue操作 ========== */

/*
** 创建upvalue对象
*/
XrUpvalue *xr_bc_upvalue_new(XrValue *location) {
    XrUpvalue *upvalue = (XrUpvalue *)xr_malloc(sizeof(XrUpvalue));
    if (upvalue == NULL) {
        return NULL;
    }
    
    xr_object_init(&upvalue->header, XR_TFUNCTION, NULL);  /* TODO: 类型 */
    upvalue->location = location;
    upvalue->closed = xr_null();
    upvalue->next = NULL;
    
    return upvalue;
}

/*
** 释放upvalue对象
*/
void xr_bc_upvalue_free(XrUpvalue *upvalue) {
    if (upvalue != NULL) {
        xr_free(upvalue);
    }
}

/*
** 捕获upvalue（Lua风格的开放/关闭机制）
** 如果upvalue已存在于链表中，重用它；否则创建新的
*/
static XrUpvalue *capture_upvalue(XrValue *location) {
    XrUpvalue *prev_upvalue = NULL;
    XrUpvalue *upvalue = vm.open_upvalues;
    
    /* 在开放upvalue链表中查找（链表按栈位置降序排列） */
    while (upvalue != NULL && upvalue->location > location) {
        prev_upvalue = upvalue;
        upvalue = upvalue->next;
    }
    
    /* 如果找到了，重用它 */
    if (upvalue != NULL && upvalue->location == location) {
        return upvalue;
    }
    
    /* 创建新的upvalue */
    XrUpvalue *created_upvalue = xr_bc_upvalue_new(location);
    created_upvalue->next = upvalue;
    
    /* 插入到链表中（保持降序） */
    if (prev_upvalue == NULL) {
        vm.open_upvalues = created_upvalue;
    } else {
        prev_upvalue->next = created_upvalue;
    }
    
    return created_upvalue;
}

/*
** 关闭upvalue（从栈转移到堆）
*/
void xr_bc_close_upvalues(XrValue *last) {
    while (vm.open_upvalues != NULL && 
           vm.open_upvalues->location >= last) {
        XrUpvalue *upvalue = vm.open_upvalues;
        
        /* 保存值到closed字段 */
        upvalue->closed = *upvalue->location;
        
        /* 更新location指向closed */
        upvalue->location = &upvalue->closed;
        
        /* 从链表移除 */
        vm.open_upvalues = upvalue->next;
    }
}

/* ========== C函数操作 ========== */

/*
** 创建C函数对象
*/
XrCFunction *xr_bc_cfunction_new(XrCFunctionPtr func, const char *name) {
    XrCFunction *cfunc = (XrCFunction *)xr_malloc(sizeof(XrCFunction));
    if (cfunc == NULL) {
        return NULL;
    }
    
    xr_object_init(&cfunc->header, XR_TCFUNCTION, NULL);
    cfunc->func = func;
    cfunc->name = name;
    
    return cfunc;
}

/*
** 释放C函数对象
*/
void xr_bc_cfunction_free(XrCFunction *cfunc) {
    if (cfunc != NULL) {
        xr_free(cfunc);
    }
}

/* ========== 闭包操作 ========== */

/*
** 创建闭包对象
*/
XrClosure *xr_bc_closure_new(Proto *proto) {
    XrClosure *closure = (XrClosure *)xr_malloc(sizeof(XrClosure));
    if (closure == NULL) {
        return NULL;
    }
    
    xr_object_init(&closure->header, XR_TFUNCTION, NULL);  /* TODO: 类型 */
    closure->proto = proto;
    closure->upvalue_count = proto->sizeupvalues;
    
    /* 分配upvalue数组 */
    if (proto->sizeupvalues > 0) {
        closure->upvalues = (XrUpvalue **)xr_malloc(sizeof(XrUpvalue*) * proto->sizeupvalues);
        for (int i = 0; i < proto->sizeupvalues; i++) {
            closure->upvalues[i] = NULL;
        }
    } else {
        closure->upvalues = NULL;
    }
    
    return closure;
}

/*
** 释放闭包对象
*/
void xr_bc_closure_free(XrClosure *closure) {
    if (closure == NULL) {
        return;
    }
    
    if (closure->upvalues != NULL) {
        xr_free(closure->upvalues);
    }
    
    xr_free(closure);
}

/* ========== VM初始化和清理 ========== */

/*
** 初始化虚拟机
*/
void xr_bc_vm_init(void) {
    vm.stack_top = vm.stack;
    vm.frame_count = 0;
    vm.open_upvalues = NULL;
    
    /* 初始化全局变量数组（Wren风格优化） */
    vm.global_count = 0;
    for (int i = 0; i < 256; i++) {
        vm.globals_array[i] = xr_null();
    }
    
    /* 初始化全局变量表（保留用于兼容性） */
    vm.globals = xr_hashmap_new();
    
    /* 初始化字符串驻留表 */
    vm.strings = xr_hashmap_new();
    
    /* GC初始化 */
    vm.objects = NULL;
    vm.bytes_allocated = 0;
    vm.next_gc = 1024 * 1024;  /* 1MB */
    
    /* 调试选项 */
    vm.trace_execution = false;
}

/*
** 释放虚拟机
*/
void xr_bc_vm_free(void) {
    /* 释放全局变量表 */
    if (vm.globals != NULL) {
        xr_hashmap_free(vm.globals);
        vm.globals = NULL;
    }
    
    /* 释放字符串驻留表 */
    if (vm.strings != NULL) {
        xr_hashmap_free(vm.strings);
        vm.strings = NULL;
    }
    
    /* 释放所有GC对象 */
    XrObject *object = vm.objects;
    while (object != NULL) {
        XrObject *next = object->next;
        /* TODO: 根据类型释放对象 */
        object = next;
    }
    vm.objects = NULL;
}

/* ========== 值操作辅助函数 ========== */

/*
** 检查两个值是否相等
*/
static bool values_equal(XrValue a, XrValue b) {
#if XR_NAN_TAGGING
    /* NaN Tagging模式 */
    if (XR_IS_NUM(a) && XR_IS_NUM(b)) {
        return XR_TO_FLOAT(a) == XR_TO_FLOAT(b);
    }
    return a == b;
#else
    /* Tagged Union模式 */
    if (a.type != b.type) {
        return false;
    }
    
    switch (a.type) {
        case XR_TNULL:
            return true;
        case XR_TBOOL:
            return a.as.b == b.as.b;
        case XR_TINT:
            return a.as.i == b.as.i;
        case XR_TFLOAT:
            return a.as.n == b.as.n;
        case XR_TSTRING:
        case XR_TFUNCTION:
        case XR_TARRAY:
        case XR_TMAP:
        case XR_TCLASS:
        case XR_TINSTANCE:
            return a.as.obj == b.as.obj;
        default:
            return false;
    }
#endif
}

/*
** 检查值是否为假
*/
static bool is_falsey(XrValue value) {
#if XR_NAN_TAGGING
    return XR_IS_NULL(value) || (XR_IS_BOOL(value) && !XR_TO_BOOL(value));
#else
    return value.type == XR_TNULL || 
           (value.type == XR_TBOOL && !value.as.b);
#endif
}

/*
** 检查值是否为真（公开API，供高阶函数使用）
*/
bool xr_bc_is_truthy(XrValue value) {
    return !is_falsey(value);
}

/* ========== VM执行循环 ========== */

/*
** VM执行循环
** v0.13.6: 支持Computed Goto优化
** v0.15.0: 改为非static，支持从C代码调用闭包
*/
InterpretResult run(void) {
    BcCallFrame *frame = &vm.frames[vm.frame_count - 1];
    Instruction inst;  /* 当前指令（需要在整个函数作用域中） */
    
#if XR_USE_COMPUTED_GOTO
    /* Computed Goto: 创建dispatch表（标签地址数组）
    ** 必须与xchunk.h中的OpCode枚举顺序完全一致！
    */
    static const void* dispatch_table[] = {
        /* 基础操作 */
        &&L_OP_MOVE,        &&L_OP_LOADI,      &&L_OP_LOADF,      &&L_OP_LOADK,
        &&L_OP_LOADNIL,     &&L_OP_LOADTRUE,   &&L_OP_LOADFALSE,
        
        /* 算术运算 */
        &&L_OP_ADD,         &&L_OP_ADDI,       &&L_UNIMPLEMENTED, /* ADDK */
        &&L_OP_SUB,         &&L_OP_SUBI,       &&L_UNIMPLEMENTED, /* SUBK */
        &&L_OP_MUL,         &&L_UNIMPLEMENTED, /* MULI */      &&L_UNIMPLEMENTED, /* MULK */
        &&L_OP_DIV,         &&L_UNIMPLEMENTED, /* DIVK */
        &&L_OP_MOD,         &&L_UNIMPLEMENTED, /* MODK */
        &&L_OP_UNM,         &&L_OP_NOT,
        
        /* 比较运算 */
        &&L_OP_EQ,          &&L_UNIMPLEMENTED, /* EQK */       &&L_UNIMPLEMENTED, /* EQI */
        &&L_OP_LT,          &&L_UNIMPLEMENTED, /* LTI */
        &&L_OP_LE,          &&L_UNIMPLEMENTED, /* LEI */
        &&L_OP_GT,          &&L_UNIMPLEMENTED, /* GTI */
        &&L_OP_GE,          &&L_UNIMPLEMENTED, /* GEI */
        
        /* 控制流 */
        &&L_OP_JMP,         &&L_OP_TEST,       &&L_OP_TESTSET,
        &&L_OP_CALL,        &&L_OP_TAILCALL,   &&L_OP_RETURN,
        
        /* 表操作 */
        &&L_UNIMPLEMENTED,  /* NEWTABLE */
        &&L_UNIMPLEMENTED,  /* GETTABLE */
        &&L_UNIMPLEMENTED,  /* GETI */
        &&L_UNIMPLEMENTED,  /* GETFIELD */
        &&L_UNIMPLEMENTED,  /* SETTABLE */
        &&L_UNIMPLEMENTED,  /* SETI */
        &&L_UNIMPLEMENTED,  /* SETFIELD */
        &&L_OP_SETLIST,
        
        /* 闭包 */
        &&L_OP_CLOSURE,     &&L_OP_GETUPVAL,   &&L_OP_SETUPVAL,   &&L_OP_CLOSE,
        
        /* OOP */
        &&L_UNIMPLEMENTED,  /* CLASS */
        &&L_UNIMPLEMENTED,  /* INHERIT */
        &&L_UNIMPLEMENTED,  /* GETPROP */
        &&L_UNIMPLEMENTED,  /* SETPROP */
        &&L_UNIMPLEMENTED,  /* GETSUPER */
        &&L_UNIMPLEMENTED,  /* INVOKE */
        &&L_UNIMPLEMENTED,  /* SUPERINVOKE */
        &&L_UNIMPLEMENTED,  /* METHOD */
        
        /* 全局变量 */
        &&L_OP_GETGLOBAL,   &&L_OP_SETGLOBAL,  &&L_UNIMPLEMENTED, /* DEFGLOBAL */
        
        /* 内置函数 */
        &&L_OP_PRINT,  /* PRINT */
        
        /* 占位符 */
        &&L_UNIMPLEMENTED,  /* NOP */
    };
#endif
    
    /* 主循环：读取第一条指令并开始dispatch 
    ** ⭐ startfunc标签：Lua风格的快速函数调用
    ** 新函数从这里开始执行，避免循环重启开销
    */
startfunc:
    inst = READ_INSTRUCTION();
    SWITCH_START();
    
#define TRACE_EXECUTION() \
    if (vm.trace_execution) { \
        printf("          "); \
        for (XrValue *slot = vm.stack; slot < vm.stack_top; slot++) { \
            printf("[ "); \
            xr_print_value(*slot); \
            printf(" ]"); \
        } \
        printf("\n"); \
        xr_disassemble_instruction(frame->closure->proto, \
                                  (int)(frame->pc - frame->closure->proto->code - 1)); \
    }
    
            CASE(OP_MOVE): {
                TRACE_EXECUTION();
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                R(a) = R(b);
                DISPATCH();
            }
            
            CASE(OP_LOADI): {
                int a = GETARG_A(inst);
                int sbx = GETARG_sBx(inst);
                R(a) = xr_int(sbx);
                DISPATCH();
            }
            
            CASE(OP_LOADF): {
                int a = GETARG_A(inst);
                int sbx = GETARG_sBx(inst);
                R(a) = xr_float((double)sbx);
                DISPATCH();
            }
            
            CASE(OP_LOADK): {
                int a = GETARG_A(inst);
                int bx = GETARG_Bx(inst);
                R(a) = K(bx);
                DISPATCH();
            }
            
            CASE(OP_LOADNIL): {
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                for (int i = 0; i <= b; i++) {
                    R(a + i) = xr_null();
                }
                DISPATCH();
            }
            
            CASE(OP_LOADTRUE): {
                int a = GETARG_A(inst);
                R(a) = xr_bool(1);
                DISPATCH();
            }
            
            CASE(OP_LOADFALSE): {
                int a = GETARG_A(inst);
                R(a) = xr_bool(0);
                DISPATCH();
            }
            
            CASE(OP_ADD): {
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int c = GETARG_C(inst);
                
                /* TODO: 完善类型检查和转换 */
                if (xr_isint(R(b)) && xr_isint(R(c))) {
                    R(a) = xr_int(xr_toint(R(b)) + xr_toint(R(c)));
                } else {
                    /* 转换为浮点数运算 */
                    double nb = xr_isint(R(b)) ? (double)xr_toint(R(b)) : xr_tofloat(R(b));
                    double nc = xr_isint(R(c)) ? (double)xr_toint(R(c)) : xr_tofloat(R(c));
                    R(a) = xr_float(nb + nc);
                }
                DISPATCH();
            }
            
            CASE(OP_ADDI): {
                /* R[A] = R[B] + sC (整数立即数优化) */
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int sc = GETARG_sC(inst);  /* 有符号立即数 */
                
                /* 直接整数运算，无类型检查 */
                R(a) = xr_int(xr_toint(R(b)) + sc);
                DISPATCH();
            }
            
            CASE(OP_ADDK): {
                /* R[A] = R[B] + K[C] (常量优化) */
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int c = GETARG_C(inst);
                XrValue kc = K(c);
                
                if (xr_isint(R(b)) && xr_isint(kc)) {
                    R(a) = xr_int(xr_toint(R(b)) + xr_toint(kc));
                } else {
                    double nb = xr_isint(R(b)) ? (double)xr_toint(R(b)) : xr_tofloat(R(b));
                    double nc = xr_isint(kc) ? (double)xr_toint(kc) : xr_tofloat(kc);
                    R(a) = xr_float(nb + nc);
                }
                DISPATCH();
            }
            
            CASE(OP_SUB): {
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int c = GETARG_C(inst);
                
                if (xr_isint(R(b)) && xr_isint(R(c))) {
                    R(a) = xr_int(xr_toint(R(b)) - xr_toint(R(c)));
                } else {
                    double nb = xr_isint(R(b)) ? (double)xr_toint(R(b)) : xr_tofloat(R(b));
                    double nc = xr_isint(R(c)) ? (double)xr_toint(R(c)) : xr_tofloat(R(c));
                    R(a) = xr_float(nb - nc);
                }
                DISPATCH();
            }
            
            CASE(OP_SUBI): {
                /* R[A] = R[B] - sC (整数立即数优化) */
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int sc = GETARG_sC(inst);
                
                R(a) = xr_int(xr_toint(R(b)) - sc);
                DISPATCH();
            }
            
            CASE(OP_SUBK): {
                /* R[A] = R[B] - K[C] (常量优化) */
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int c = GETARG_C(inst);
                XrValue kc = K(c);
                
                if (xr_isint(R(b)) && xr_isint(kc)) {
                    R(a) = xr_int(xr_toint(R(b)) - xr_toint(kc));
                } else {
                    double nb = xr_isint(R(b)) ? (double)xr_toint(R(b)) : xr_tofloat(R(b));
                    double nc = xr_isint(kc) ? (double)xr_toint(kc) : xr_tofloat(kc);
                    R(a) = xr_float(nb - nc);
                }
                DISPATCH();
            }
            
            CASE(OP_MUL): {
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int c = GETARG_C(inst);
                
                if (xr_isint(R(b)) && xr_isint(R(c))) {
                    R(a) = xr_int(xr_toint(R(b)) * xr_toint(R(c)));
                } else {
                    double nb = xr_isint(R(b)) ? (double)xr_toint(R(b)) : xr_tofloat(R(b));
                    double nc = xr_isint(R(c)) ? (double)xr_toint(R(c)) : xr_tofloat(R(c));
                    R(a) = xr_float(nb * nc);
                }
                DISPATCH();
            }
            
            CASE(OP_MULI): {
                /* R[A] = R[B] * sC (整数立即数优化) */
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int sc = GETARG_sC(inst);
                
                R(a) = xr_int(xr_toint(R(b)) * sc);
                DISPATCH();
            }
            
            CASE(OP_MULK): {
                /* R[A] = R[B] * K[C] (常量优化) */
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int c = GETARG_C(inst);
                XrValue kc = K(c);
                
                if (xr_isint(R(b)) && xr_isint(kc)) {
                    R(a) = xr_int(xr_toint(R(b)) * xr_toint(kc));
                } else {
                    double nb = xr_isint(R(b)) ? (double)xr_toint(R(b)) : xr_tofloat(R(b));
                    double nc = xr_isint(kc) ? (double)xr_toint(kc) : xr_tofloat(kc);
                    R(a) = xr_float(nb * nc);
                }
                DISPATCH();
            }
            
            CASE(OP_DIV): {
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int c = GETARG_C(inst);
                
                /* 除法总是返回浮点数 */
                double nb = xr_isint(R(b)) ? (double)xr_toint(R(b)) : xr_tofloat(R(b));
                double nc = xr_isint(R(c)) ? (double)xr_toint(R(c)) : xr_tofloat(R(c));
                
                if (nc == 0.0) {
                    xr_bc_runtime_error("Division by zero");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                R(a) = xr_float(nb / nc);
                DISPATCH();
            }
            
            CASE(OP_MOD): {
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int c = GETARG_C(inst);
                
                if (xr_isint(R(b)) && xr_isint(R(c))) {
                    xr_Integer divisor = xr_toint(R(c));
                    if (divisor == 0) {
                        xr_bc_runtime_error("Modulo by zero");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    R(a) = xr_int(xr_toint(R(b)) % divisor);
                } else {
                    double nb = xr_isint(R(b)) ? (double)xr_toint(R(b)) : xr_tofloat(R(b));
                    double nc = xr_isint(R(c)) ? (double)xr_toint(R(c)) : xr_tofloat(R(c));
                    R(a) = xr_float(fmod(nb, nc));
                }
                DISPATCH();
            }
            
            CASE(OP_UNM): {
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                
                if (xr_isint(R(b))) {
                    R(a) = xr_int(-xr_toint(R(b)));
                } else if (xr_isfloat(R(b))) {
                    R(a) = xr_float(-xr_tofloat(R(b)));
                } else {
                    xr_bc_runtime_error("Operand must be a number");
                    return INTERPRET_RUNTIME_ERROR;
                }
                DISPATCH();
            }
            
            CASE(OP_NOT): {
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                R(a) = xr_bool(is_falsey(R(b)));
                DISPATCH();
            }
            
            CASE(OP_EQ): {
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int k = GETARG_C(inst);  /* 条件标志 */
                
                if (values_equal(R(a), R(b)) != k) {
                    frame->pc++;  /* 跳过下一条指令 */
                }
                DISPATCH();
            }
            
            CASE(OP_LT): {
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int k = GETARG_C(inst);
                
                /* TODO: 完善比较逻辑 */
                bool result = false;
                if (xr_isint(R(a)) && xr_isint(R(b))) {
                    result = xr_toint(R(a)) < xr_toint(R(b));
                } else {
                    double na = xr_isint(R(a)) ? (double)xr_toint(R(a)) : xr_tofloat(R(a));
                    double nb = xr_isint(R(b)) ? (double)xr_toint(R(b)) : xr_tofloat(R(b));
                    result = na < nb;
                }
                
                if (result != k) {
                    frame->pc++;
                }
                DISPATCH();
            }
            
            CASE(OP_LE): {
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int k = GETARG_C(inst);
                
                bool result = false;
                if (xr_isint(R(a)) && xr_isint(R(b))) {
                    result = xr_toint(R(a)) <= xr_toint(R(b));
                } else {
                    double na = xr_isint(R(a)) ? (double)xr_toint(R(a)) : xr_tofloat(R(a));
                    double nb = xr_isint(R(b)) ? (double)xr_toint(R(b)) : xr_tofloat(R(b));
                    result = na <= nb;
                }
                
                if (result != k) {
                    frame->pc++;
                }
                DISPATCH();
            }
            
            CASE(OP_GT): {
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int k = GETARG_C(inst);
                
                bool result = false;
                if (xr_isint(R(a)) && xr_isint(R(b))) {
                    result = xr_toint(R(a)) > xr_toint(R(b));
                } else {
                    double na = xr_isint(R(a)) ? (double)xr_toint(R(a)) : xr_tofloat(R(a));
                    double nb = xr_isint(R(b)) ? (double)xr_toint(R(b)) : xr_tofloat(R(b));
                    result = na > nb;
                }
                
                if (result != k) {
                    frame->pc++;
                }
                DISPATCH();
            }
            
            CASE(OP_GE): {
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int k = GETARG_C(inst);
                
                bool result = false;
                if (xr_isint(R(a)) && xr_isint(R(b))) {
                    result = xr_toint(R(a)) >= xr_toint(R(b));
                } else {
                    double na = xr_isint(R(a)) ? (double)xr_toint(R(a)) : xr_tofloat(R(a));
                    double nb = xr_isint(R(b)) ? (double)xr_toint(R(b)) : xr_tofloat(R(b));
                    result = na >= nb;
                }
                
                if (result != k) {
                    frame->pc++;
                }
                DISPATCH();
            }
            
            CASE(OP_JMP): {
                int sj = GETARG_sJ(inst);
                frame->pc += sj;
                DISPATCH();
            }
            
            CASE(OP_TEST): {
                int a = GETARG_A(inst);
                int k = GETARG_B(inst);
                
                if (is_falsey(R(a)) == k) {
                    frame->pc++;  /* 跳过下一条指令 */
                }
                DISPATCH();
            }
            
            CASE(OP_TESTSET): {
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int k = GETARG_C(inst);
                
                if (is_falsey(R(b)) != k) {
                    R(a) = R(b);
                    frame->pc++;  /* 跳过下一条指令 */
                }
                DISPATCH();
            }
            
            CASE(OP_GETGLOBAL): {
                /* Wren风格优化：Bx现在是全局变量的固定索引，而非常量索引 */
                int a = GETARG_A(inst);
                int global_index = GETARG_Bx(inst);
                
                /* 直接从数组读取全局变量（O(1)） */
                if (global_index < vm.global_count) {
                    R(a) = vm.globals_array[global_index];
                } else {
                    R(a) = xr_null();  /* 未定义的全局变量 */
                }
                DISPATCH();
            }
            
            CASE(OP_SETGLOBAL): {
                /* Wren风格优化：Bx现在是全局变量的固定索引，而非常量索引 */
                int a = GETARG_A(inst);
                int global_index = GETARG_Bx(inst);
                
                /* 直接写入数组（O(1)） */
                vm.globals_array[global_index] = R(a);
                
                /* 更新全局变量计数 */
                if (global_index >= vm.global_count) {
                    vm.global_count = global_index + 1;
                }
                
                /* 同时更新哈希表（用于兼容性/调试） */
                /* TODO: 后续可以完全移除哈希表 */
                DISPATCH();
            }
            
            CASE(OP_CLOSURE): {
                int a = GETARG_A(inst);
                int bx = GETARG_Bx(inst);
                
                /* 安全检查 */
                if (frame->closure == NULL || frame->closure->proto == NULL) {
                    xr_bc_runtime_error("Null closure or proto in CLOSURE");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (bx >= frame->closure->proto->sizeprotos) {
                    xr_bc_runtime_error("Proto index %d out of bounds (max %d)", 
                                     bx, frame->closure->proto->sizeprotos);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                /* 获取函数原型 */
                Proto *proto = frame->closure->proto->protos[bx];
                if (proto == NULL) {
                    xr_bc_runtime_error("Null proto at index %d", bx);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                /* 创建闭包 */
                XrClosure *closure = xr_bc_closure_new(proto);
                
                /* 捕获upvalues（使用Lua风格的capture机制） */
                for (int i = 0; i < proto->sizeupvalues; i++) {
                    UpvalInfo *uvinfo = &proto->upvalues[i];
                    if (uvinfo->is_local) {
                        /* 捕获局部变量（使用capture_upvalue来重用已有的upvalue） */
                        closure->upvalues[i] = capture_upvalue(&R(uvinfo->index));
                    } else {
                        /* 继承外层upvalue */
                        closure->upvalues[i] = frame->closure->upvalues[uvinfo->index];
                    }
                }
                
                /* 存储闭包（使用正确的闭包值表示） */
                R(a) = xr_value_from_closure(closure);
                DISPATCH();
            }
            
            CASE(OP_GETUPVAL): {
                /* R[A] = UpValue[B] */
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                
                /* 安全检查 */
                if (frame->closure == NULL) {
                    xr_bc_runtime_error("Null closure in GETUPVAL");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (b >= frame->closure->upvalue_count) {
                    xr_bc_runtime_error("Upvalue index %d out of bounds (max %d)", 
                                     b, frame->closure->upvalue_count);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                /* 从当前闭包的upvalue数组中获取值 */
                XrUpvalue *upvalue = frame->closure->upvalues[b];
                if (upvalue == NULL) {
                    xr_bc_runtime_error("Null upvalue at index %d", b);
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (upvalue->location == NULL) {
                    xr_bc_runtime_error("Null upvalue location at index %d", b);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                R(a) = *upvalue->location;
                DISPATCH();
            }
            
            CASE(OP_SETUPVAL): {
                /* UpValue[B] = R[A] */
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                
                /* 安全检查 */
                if (frame->closure == NULL) {
                    xr_bc_runtime_error("Null closure in SETUPVAL");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (b >= frame->closure->upvalue_count) {
                    xr_bc_runtime_error("Upvalue index %d out of bounds", b);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                /* 设置upvalue的值 */
                XrUpvalue *upvalue = frame->closure->upvalues[b];
                if (upvalue == NULL) {
                    xr_bc_runtime_error("Null upvalue at index %d", b);
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (upvalue->location == NULL) {
                    xr_bc_runtime_error("Null upvalue location at index %d", b);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                *upvalue->location = R(a);
                DISPATCH();
            }
            
            CASE(OP_CLOSE): {
                /* close upvalues >= R[A] */
                int a = GETARG_A(inst);
                xr_bc_close_upvalues(&R(a));
                DISPATCH();
            }
            
            CASE(OP_PRINT): {
                /* print(R[A]) - 打印寄存器值 */
                int a = GETARG_A(inst);
                xr_print_value(R(a));
                printf("\n");
                DISPATCH();
            }
            
            CASE(OP_NEWTABLE): {
                /* R[A] = {} - 创建新数组/表 */
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);  /* 数组大小提示 */
                
                /* 创建数组 */
                XrArray *array = (b > 0) ? xr_array_with_capacity(b) : xr_array_new();
                
                /* 存储数组 */
                R(a) = xr_value_from_array(array);
                DISPATCH();
            }
            
            CASE(OP_GETTABLE): {
                /* R[A] = R[B][R[C]] - 获取表元素 */
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int c = GETARG_C(inst);
                
                /* 获取数组对象 */
                XrValue table_val = R(b);
                if (!xr_isarray(table_val)) {
                    xr_bc_runtime_error("Attempt to index a non-array value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                XrArray *array = xr_to_array(table_val);
                
                /* 获取索引 */
                XrValue index_val = R(c);
                if (!xr_isint(index_val)) {
                    xr_bc_runtime_error("Array index must be an integer");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                int index = (int)xr_toint(index_val);
                R(a) = xr_array_get(array, index);
                DISPATCH();
            }
            
            CASE(OP_SETTABLE): {
                /* R[A][R[B]] = R[C] - 设置表元素 */
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int c = GETARG_C(inst);
                
                /* 获取数组对象 */
                XrValue table_val = R(a);
                if (!xr_isarray(table_val)) {
                    xr_bc_runtime_error("Attempt to index a non-array value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                XrArray *array = xr_to_array(table_val);
                
                /* 获取索引 */
                XrValue index_val = R(b);
                if (!xr_isint(index_val)) {
                    xr_bc_runtime_error("Array index must be an integer");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                int index = (int)xr_toint(index_val);
                xr_array_set(array, index, R(c));
                DISPATCH();
            }
            
            CASE(OP_SETLIST): {
                /* R[A][i] = R[A+i], 1 <= i <= B - 批量设置数组元素 */
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                
                /* 获取数组对象 */
                XrValue array_val = R(a);
                if (!xr_isarray(array_val)) {
                    xr_bc_runtime_error("SETLIST requires an array");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                XrArray *array = xr_to_array(array_val);
                
                /* 批量设置元素（xr_array_set会自动扩展数组） */
                for (int i = 1; i <= b; i++) {
                    xr_array_set(array, i - 1, R(a + i));
                }
                DISPATCH();
            }
            
            CASE(OP_CALL): {
                TRACE_EXECUTION();
                int a = GETARG_A(inst);
                int nargs = GETARG_B(inst);
                
                XrValue func_val = R(a);
                
                /* ⭐ P5优化：C函数分离调用（Lua/Wren风格）
                ** C函数走快速路径，无需创建CallFrame
                */
                if (xr_value_is_cfunction(func_val)) {
                    /* C函数快速路径 */
                    XrCFunction *cfunc = xr_value_to_cfunction(func_val);
                    
                    /* 调用C函数（参数从R[a+1]开始） */
                    XrValue result = cfunc->func(&vm, &R(a + 1), nargs);
                    
                    /* 检查是否出错（可以通过返回值类型判断）*/
                    if (xr_isnull(result) && nargs < 0) {  /* 错误标志 */
                        xr_bc_runtime_error("C function '%s' failed", cfunc->name);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    /* 返回值放在R[a]（函数位置） */
                    R(a) = result;
                    
                    /* C函数调用完成，继续执行 */
                    DISPATCH();
                }
                
                /* Xray闭包：原有路径 */
                if (!xr_isfunction(func_val)) {
                    xr_bc_runtime_error("Attempt to call a non-function value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                /* 从对象指针获取闭包 */
                XrClosure *closure = xr_value_to_closure(func_val);
                
                /* 快速路径：参数数量匹配（绝大多数情况） */
                if (likely(nargs == closure->proto->numparams)) {
                    /* 检查栈空间 */
                    if (unlikely(vm.frame_count >= FRAMES_MAX)) {
                        xr_bc_runtime_error("Stack overflow");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    /* 创建新的调用帧 */
                    BcCallFrame *new_frame = &vm.frames[vm.frame_count++];
                    new_frame->closure = closure;
                    new_frame->pc = closure->proto->code;
                    new_frame->base = frame->base + a + 1;  /* 参数从R[a+1]开始 */
                    
                    /* ⭐ Phase 1优化：直接跳转到startfunc
                    ** 避免DISPATCH()的循环开销，直接开始执行新函数
                    */
                    frame = new_frame;
                    goto startfunc;
                } else {
                    /* 慢速路径：参数数量不匹配（错误情况） */
                    xr_bc_runtime_error("Expected %d arguments but got %d",
                                     closure->proto->numparams, nargs);
                    return INTERPRET_RUNTIME_ERROR;
                }
            }
            
            CASE(OP_TAILCALL): {
                TRACE_EXECUTION();
                int a = GETARG_A(inst);
                int nargs = GETARG_B(inst);
                
                /* ⭐ Phase 2: 尾调用优化
                ** 关键：复用当前栈帧，不增加调用深度
                ** 这允许无限尾递归而不栈溢出
                */
                
                /* 获取被调用的函数 */
                XrValue func_val = R(a);
                
                /* 类型检查 */
                if (!xr_isfunction(func_val)) {
                    xr_bc_runtime_error("Attempt to call a non-function value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                XrClosure *new_closure = xr_value_to_closure(func_val);
                
                /* 参数数量检查 */
                if (unlikely(nargs != new_closure->proto->numparams)) {
                    xr_bc_runtime_error("Expected %d arguments but got %d",
                                     new_closure->proto->numparams, nargs);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                /* ⭐ 诊断：检查栈空间 */
                size_t current_stack_offset = frame->base - vm.stack;
                size_t required_stack_size = current_stack_offset + new_closure->proto->maxstacksize;
                
                if (required_stack_size > STACK_MAX) {
                    fprintf(stderr, "DEBUG: Stack overflow detected!\n");
                    fprintf(stderr, "  current offset: %zu\n", current_stack_offset);
                    fprintf(stderr, "  maxstacksize: %d\n", new_closure->proto->maxstacksize);
                    fprintf(stderr, "  required: %zu, max: %d\n", required_stack_size, STACK_MAX);
                    fprintf(stderr, "  frame_count: %d\n", vm.frame_count);
                    xr_bc_runtime_error("Stack overflow in tail call");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                /* ⭐ 关键步骤1: 关闭当前frame的upvalues */
                xr_bc_close_upvalues(frame->base);
                
                /* ⭐ 关键步骤2: 移动参数到frame->base位置
                ** 使用memmove处理可能重叠的内存区域
                */
                if (nargs > 0) {
                    memmove(frame->base, &R(a + 1), sizeof(XrValue) * nargs);
                }
                
                /* ⭐ 关键步骤3: 更新frame的closure和PC
                ** 注意：不改变frame_count，复用当前栈帧！
                */
                frame->closure = new_closure;
                frame->pc = new_closure->proto->code;
                
                /* ⭐ 关键步骤4: 更新栈顶指针
                ** 不应该累积增长！应该保持在frame->base + maxstacksize
                */
                vm.stack_top = frame->base + new_closure->proto->maxstacksize;
                
                /* ⭐ 关键步骤5: 直接跳转到startfunc开始执行
                ** 不增加调用深度，实现真正的尾调用优化
                */
                goto startfunc;
            }
            
            CASE(OP_RETURN): {
                TRACE_EXECUTION();
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                
                /* 获取返回值 */
                XrValue result = (b > 0) ? R(a) : xr_null();
                
                /* 关闭upvalues */
                xr_bc_close_upvalues(frame->base);
                
                /* 弹出调用帧 */
                vm.frame_count--;
                
                if (vm.frame_count == 0) {
                    /* 顶层脚本返回 */
                    vm.stack_top = vm.stack;
                    return INTERPRET_OK;
                }
                
                /* 计算返回位置（调用者的寄存器） */
                /* 假设调用是 CALL R[x] ... */
                /* 返回值应该放在R[x]的位置 */
                XrValue *return_slot = frame->base - 1;  /* 函数自身的位置 */
                
                /* 恢复调用者frame */
                frame = &vm.frames[vm.frame_count - 1];
                
                /* 将返回值放到正确位置 */
                *return_slot = result;
                
                /* 更新栈顶 */
                vm.stack_top = return_slot + 1;
                
                /* ⭐ Phase 1优化：返回后直接跳转到startfunc继续执行
                ** 避免DISPATCH()的循环开销
                */
                goto startfunc;
            }
            
    SWITCH_END();  /* 在switch模式下添加default和closing braces */
    
#if XR_USE_COMPUTED_GOTO
L_UNIMPLEMENTED:
    /* 未实现的指令 */
    xr_bc_runtime_error("Unimplemented opcode %d", GET_OPCODE(inst));
    return INTERPRET_RUNTIME_ERROR;
#endif
}

/* ========== C代码调用闭包API ========== */

/*
** 从C代码调用Xray闭包
** 用于高阶函数（map/filter/reduce等）
*/
XrValue xr_bc_call_closure(XrClosure *closure, XrValue *args, int nargs) {
    /* 参数检查 */
    if (closure == NULL || closure->proto == NULL) {
        return xr_null();
    }
    
    /* 检查参数数量 */
    if (nargs != closure->proto->numparams) {
        fprintf(stderr, "Expected %d arguments but got %d\n",
                closure->proto->numparams, nargs);
        return xr_null();
    }
    
    /* 保存当前栈顶 */
    XrValue *saved_stack_top = vm.stack_top;
    int saved_frame_count = vm.frame_count;
    
    /* 创建新的调用帧 */
    if (vm.frame_count >= FRAMES_MAX) {
        fprintf(stderr, "Stack overflow in callback\n");
        return xr_null();
    }
    
    BcCallFrame *frame = &vm.frames[vm.frame_count++];
    frame->closure = closure;
    frame->pc = closure->proto->code;
    frame->base = vm.stack_top;  /* 新frame从当前栈顶开始 */
    
    /* 复制参数到栈上 */
    for (int i = 0; i < nargs; i++) {
        vm.stack_top[i] = args[i];
    }
    
    /* 更新栈顶 */
    vm.stack_top = frame->base + closure->proto->maxstacksize;
    
    /* 执行字节码 */
    InterpretResult result = run();
    
    /* 获取返回值（在frame->base - 1位置）*/
    XrValue return_value = (vm.frame_count == saved_frame_count) ? 
                           *(frame->base - 1) : xr_null();
    
    /* 恢复栈状态 */
    vm.stack_top = saved_stack_top;
    vm.frame_count = saved_frame_count;
    
    return return_value;
}

/* ========== 解释执行API ========== */

/*
** 执行函数原型
*/
InterpretResult xr_bc_interpret_proto(Proto *proto) {
    /* 创建顶层闭包 */
    XrClosure *closure = xr_bc_closure_new(proto);
    if (closure == NULL) {
        return INTERPRET_RUNTIME_ERROR;
    }
    
    /* 不需要压栈 - 直接创建调用帧 */
    vm.stack_top = vm.stack;
    
    /* 创建调用帧 */
    BcCallFrame *frame = &vm.frames[vm.frame_count++];
    frame->closure = closure;
    frame->pc = proto->code;
    frame->base = vm.stack;
    
    /* 执行 */
    return run();
}

/*
** 执行源代码
*/
InterpretResult xr_bc_interpret(const char *source) {
    /* TODO: 编译源代码到Proto */
    /* 目前需要xcompiler模块 */
    fprintf(stderr, "Compiler not yet implemented\n");
    return INTERPRET_COMPILE_ERROR;
}


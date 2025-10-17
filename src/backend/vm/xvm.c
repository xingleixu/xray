/*
** xvm.c
** Xray 寄存器虚拟机实现
** v0.18.0 - 高级优化（快速路径、类型特化、循环优化）
** v0.19.0 - OOP支持（类、方法、运算符重载）
*/

#include "xvm.h"
#include "xdebug.h"
#include "xmem.h"
#include "xstring.h"
#include "xarray.h"
#include "xclass.h"      /* v0.19.0：类对象 */
#include "xinstance.h"   /* v0.19.0：实例对象 */
#include "xmethod.h"     /* v0.19.0：方法对象 */
#include "xsymbol.h"     /* v0.20.0：Symbol系统 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

/* 全局VM实例已删除 - 现在通过参数传递 */

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

/* 快速路径优化已移除 - 实测反而增加开销 */

/* ========== VM 指令分发 ========== */

/*
** 指令分发使用传统 Switch 模式
** 
** 性能测试结论（2025-10-16, Apple M1 Pro）:
**   • Switch 模式: 0.70s (Fibonacci 35)
**   • Computed Goto: 0.77s (慢 9%)
** 
** Switch 优势：
**   1. 性能更优（现代CPU分支预测强大）
**   2. 代码简单易维护
**   3. 编译器类型检查（避免跳转表bug）
**   4. 所有平台都支持
*/

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
void xr_bc_runtime_error(VM *vm, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);
    
    /* 打印调用栈 */
    for (int i = vm->frame_count - 1; i >= 0; i--) {
        BcCallFrame *frame = &vm->frames[i];
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
    vm->stack_top = vm->stack;
    vm->frame_count = 0;
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
    
    xr_object_init(&upvalue->header, XR_TFUNCTION, NULL);  /* Upvalue类型信息由GC阶段完善 */
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
static XrUpvalue *capture_upvalue(VM *vm, XrValue *location) {
    XrUpvalue *prev_upvalue = NULL;
    XrUpvalue *upvalue = vm->open_upvalues;
    
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
        vm->open_upvalues = created_upvalue;
    } else {
        prev_upvalue->next = created_upvalue;
    }
    
    return created_upvalue;
}

/*
** 关闭upvalue（从栈转移到堆）
*/
void xr_bc_close_upvalues(VM *vm, XrValue *last) {
    while (vm->open_upvalues != NULL && 
           vm->open_upvalues->location >= last) {
        XrUpvalue *upvalue = vm->open_upvalues;
        
        /* 保存值到closed字段 */
        upvalue->closed = *upvalue->location;
        
        /* 更新location指向closed */
        upvalue->location = &upvalue->closed;
        
        /* 从链表移除 */
        vm->open_upvalues = upvalue->next;
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
    
    xr_object_init(&closure->header, XR_TFUNCTION, NULL);  /* 闭包类型信息由类型系统完善 */
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
void xr_bc_vm_init(VM *vm) {
    vm->stack_top = vm->stack;
    vm->frame_count = 0;
    vm->open_upvalues = NULL;
    
    /* 初始化全局变量数组（Wren风格优化） */
    vm->global_count = 0;
    for (int i = 0; i < 256; i++) {
        vm->globals_array[i] = xr_null();
    }
    
    /* 全局变量使用数组，无需哈希表 */
    
    /* 初始化字符串驻留表 */
    vm->strings = xr_hashmap_new();
    
    /* GC初始化 */
    vm->objects = NULL;
    vm->bytes_allocated = 0;
    vm->next_gc = 1024 * 1024;  /* 1MB */
    
    /* 调试选项 */
    vm->trace_execution = false;
}

/*
** 释放虚拟机
*/
void xr_bc_vm_free(VM *vm) {
    /* 全局变量使用数组，无需释放哈希表 */
    
    /* 释放字符串驻留表 */
    if (vm->strings != NULL) {
        xr_hashmap_free(vm->strings);
        vm->strings = NULL;
    }
    
    /* 释放所有GC对象 */
    XrObject *object = vm->objects;
    while (object != NULL) {
        XrObject *next = object->next;
        /* 对象释放：当前由xmem管理，GC阶段将自动处理 */
        object = next;
    }
    vm->objects = NULL;
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
InterpretResult run(VM *vm) {
    BcCallFrame *frame = &vm->frames[vm->frame_count - 1];
    Instruction inst;  /* 当前指令（需要在整个函数作用域中） */
    
    /* 主循环：读取第一条指令并开始dispatch 
    ** ⭐ startfunc标签：Lua风格的快速函数调用
    ** 新函数从这里开始执行，避免循环重启开销
    */
startfunc:
    inst = READ_INSTRUCTION();
    
    /* Switch 指令分发循环 */
    for (;;) {
        OpCode op = GET_OPCODE(inst);
        switch (op) {
    
#define TRACE_EXECUTION() \
    if (vm->trace_execution) { \
        printf("          "); \
        for (XrValue *slot = vm->stack; slot < vm->stack_top; slot++) { \
            printf("[ "); \
            xr_print_value(*slot); \
            printf(" ]"); \
        } \
        printf("\n"); \
        xr_disassemble_instruction(frame->closure->proto, \
                                  (int)(frame->pc - frame->closure->proto->code - 1)); \
    }
    
            case OP_MOVE: {
                TRACE_EXECUTION();
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                R(a) = R(b);
                break;
            }
            
            case OP_LOADI: {
                int a = GETARG_A(inst);
                int sbx = GETARG_sBx(inst);
                R(a) = xr_int(sbx);
                break;
            }
            
            case OP_LOADF: {
                int a = GETARG_A(inst);
                int sbx = GETARG_sBx(inst);
                R(a) = xr_float((double)sbx);
                break;
            }
            
            case OP_LOADK: {
                int a = GETARG_A(inst);
                int bx = GETARG_Bx(inst);
                R(a) = K(bx);
                break;
            }
            
            case OP_LOADNIL: {
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                for (int i = 0; i <= b; i++) {
                    R(a + i) = xr_null();
                }
                break;
            }
            
            case OP_LOADTRUE: {
                int a = GETARG_A(inst);
                R(a) = xr_bool(1);
                break;
            }
            
            case OP_LOADFALSE: {
                int a = GETARG_A(inst);
                R(a) = xr_bool(0);
                break;
            }
            
            case OP_ADD: {
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int c = GETARG_C(inst);
                
                /* v0.19.0：检查是否为类实例，支持运算符重载 */
                if (xr_value_is_instance(R(b))) {
                    /* v0.20.0: 左操作数是类实例，通过symbol查找operator+ */
                    XrInstance *inst_obj = xr_value_to_instance(R(b));
                    XrMethod *op_method = xr_class_lookup_method_by_symbol(inst_obj->klass, SYMBOL_OP_ADD);
                    
                    if (op_method != NULL && op_method->func != NULL) {
                        /* 找到运算符方法，通过字节码调用（类似OP_INVOKE）*/
                        Proto *proto = (Proto*)op_method->func;
                        
                        /* 检查参数数量（运算符方法：this + other）*/
                        if (1 + 1 != proto->numparams) {  /* this + other = 2个参数 */
                            xr_bc_runtime_error(vm, "Operator + expects 1 argument");
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        /* 检查栈空间 */
                        if (vm->frame_count >= FRAMES_MAX) {
                            xr_bc_runtime_error(vm, "Stack overflow");
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        
                        /* 创建闭包对象 */
                        XrClosure *closure = (XrClosure*)gc_alloc(sizeof(XrClosure), OBJ_CLOSURE);
                        closure->header.type = XR_TFUNCTION;
                        closure->header.next = vm->objects;
                        closure->header.marked = false;
                        closure->proto = proto;
                        closure->upvalue_count = 0;
                        closure->upvalues = NULL;
                        vm->objects = (XrObject*)closure;
                        
                        /* 设置参数：R[a+1] = this, R[a+2] = other */
                        R(a + 1) = R(b);  /* this */
                        R(a + 2) = R(c);  /* other */
                        
                        /* 创建新的调用帧 */
                        BcCallFrame *new_frame = &vm->frames[vm->frame_count++];
                        new_frame->closure = closure;
                        new_frame->pc = proto->code;
                        new_frame->base = frame->base + a + 1;  /* 参数从R[a+1]开始 */
                        
                        /* 跳转到新函数执行 */
                        frame = new_frame;
                        goto startfunc;
                    }
                    /* 没有找到运算符方法，继续尝试内置运算 */
                }
                
                /* 内置类型加法 */
                if (xr_isint(R(b)) && xr_isint(R(c))) {
                    R(a) = xr_int(xr_toint(R(b)) + xr_toint(R(c)));
                } else if ((xr_isint(R(b)) || xr_isfloat(R(b))) && (xr_isint(R(c)) || xr_isfloat(R(c)))) {
                    double nb = xr_isint(R(b)) ? (double)xr_toint(R(b)) : xr_tofloat(R(b));
                    double nc = xr_isint(R(c)) ? (double)xr_toint(R(c)) : xr_tofloat(R(c));
                    R(a) = xr_float(nb + nc);
                } else {
                    xr_bc_runtime_error(vm, "类型错误：加法操作数必须是数字或定义了operator+的类实例");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            
            case OP_ADDI: {
                /* R[A] = R[B] + sC (整数立即数优化) */
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int sc = GETARG_sC(inst);  /* 有符号立即数 */
                
                /* 直接整数运算，无类型检查 */
                R(a) = xr_int(xr_toint(R(b)) + sc);
                break;
            }
            
            case OP_ADDK: {
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
                break;
            }
            
            case OP_SUB: {
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
                break;
            }
            
            case OP_SUBI: {
                /* R[A] = R[B] - sC (整数立即数优化) */
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int sc = GETARG_sC(inst);
                
                R(a) = xr_int(xr_toint(R(b)) - sc);
                break;
            }
            
            case OP_SUBK: {
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
                break;
            }
            
            case OP_MUL: {
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
                break;
            }
            
            case OP_MULI: {
                /* R[A] = R[B] * sC (整数立即数优化) */
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int sc = GETARG_sC(inst);
                
                R(a) = xr_int(xr_toint(R(b)) * sc);
                break;
            }
            
            case OP_MULK: {
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
                break;
            }
            
            case OP_DIV: {
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int c = GETARG_C(inst);
                
                /* 除法总是返回浮点数 */
                double nb = xr_isint(R(b)) ? (double)xr_toint(R(b)) : xr_tofloat(R(b));
                double nc = xr_isint(R(c)) ? (double)xr_toint(R(c)) : xr_tofloat(R(c));
                
                if (nc == 0.0) {
                    xr_bc_runtime_error(vm, "Division by zero");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                R(a) = xr_float(nb / nc);
                break;
            }
            
            case OP_MOD: {
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int c = GETARG_C(inst);
                
                if (xr_isint(R(b)) && xr_isint(R(c))) {
                    xr_Integer divisor = xr_toint(R(c));
                    if (divisor == 0) {
                        xr_bc_runtime_error(vm, "Modulo by zero");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    R(a) = xr_int(xr_toint(R(b)) % divisor);
                } else {
                    double nb = xr_isint(R(b)) ? (double)xr_toint(R(b)) : xr_tofloat(R(b));
                    double nc = xr_isint(R(c)) ? (double)xr_toint(R(c)) : xr_tofloat(R(c));
                    R(a) = xr_float(fmod(nb, nc));
                }
                break;
            }
            
            case OP_UNM: {
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                
                if (xr_isint(R(b))) {
                    R(a) = xr_int(-xr_toint(R(b)));
                } else if (xr_isfloat(R(b))) {
                    R(a) = xr_float(-xr_tofloat(R(b)));
                } else {
                    xr_bc_runtime_error(vm, "Operand must be a number");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            
            case OP_NOT: {
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                R(a) = xr_bool(is_falsey(R(b)));
                break;
            }
            
            case OP_EQ: {
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int k = GETARG_C(inst);  /* 条件标志 */
                
                if (values_equal(R(a), R(b)) != k) {
                    frame->pc++;  /* 跳过下一条指令 */
                }
                break;
            }
            
            case OP_LT: {
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int k = GETARG_C(inst);
                
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
                break;
            }
            
            case OP_LTI: {
                /* ⭐ v0.18.0优化：立即数比较 */
                int a = GETARG_A(inst);
                int sb = GETARG_sB(inst);
                int k = GETARG_C(inst);
                
                bool result = false;
                if (xr_isint(R(a))) {
                    result = xr_toint(R(a)) < sb;
                } else if (xr_isfloat(R(a))) {
                    result = xr_tofloat(R(a)) < (double)sb;
                } else {
                    result = false;
                }
                
                if (result != k) {
                    frame->pc++;
                }
                break;
            }
            
            case OP_LE: {
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
                break;
            }
            
            case OP_LEI: {
                /* ⭐ v0.18.0优化：立即数比较，避免LOADK */
                int a = GETARG_A(inst);
                int sb = GETARG_sB(inst);  /* 有符号立即数 */
                int k = GETARG_C(inst);
                
                bool result = false;
                if (xr_isint(R(a))) {
                    result = xr_toint(R(a)) <= sb;
                } else if (xr_isfloat(R(a))) {
                    result = xr_tofloat(R(a)) <= (double)sb;
                } else {
                    result = false;  /* 其他类型视为不满足 */
                }
                
                if (result != k) {
                    frame->pc++;
                }
                break;
            }
            
            case OP_GT: {
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
                break;
            }
            
            case OP_GTI: {
                /* ⭐ v0.18.0优化：立即数比较 */
                int a = GETARG_A(inst);
                int sb = GETARG_sB(inst);
                int k = GETARG_C(inst);
                
                bool result = false;
                if (xr_isint(R(a))) {
                    result = xr_toint(R(a)) > sb;
                } else if (xr_isfloat(R(a))) {
                    result = xr_tofloat(R(a)) > (double)sb;
                } else {
                    result = false;
                }
                
                if (result != k) {
                    frame->pc++;
                }
                break;
            }
            
            case OP_GE: {
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
                break;
            }
            
            case OP_GEI: {
                /* ⭐ v0.18.0优化：立即数比较 */
                int a = GETARG_A(inst);
                int sb = GETARG_sB(inst);
                int k = GETARG_C(inst);
                
                bool result = false;
                if (xr_isint(R(a))) {
                    result = xr_toint(R(a)) >= sb;
                } else if (xr_isfloat(R(a))) {
                    result = xr_tofloat(R(a)) >= (double)sb;
                } else {
                    result = false;
                }
                
                if (result != k) {
                    frame->pc++;
                }
                break;
            }
            
            case OP_JMP: {
                int sj = GETARG_sJ(inst);
                frame->pc += sj;
                break;
            }
            
            case OP_TEST: {
                int a = GETARG_A(inst);
                int k = GETARG_B(inst);
                
                if (is_falsey(R(a)) == k) {
                    frame->pc++;  /* 跳过下一条指令 */
                }
                break;
            }
            
            case OP_TESTSET: {
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int k = GETARG_C(inst);
                
                if (is_falsey(R(b)) != k) {
                    R(a) = R(b);
                    frame->pc++;  /* 跳过下一条指令 */
                }
                break;
            }
            
            case OP_GETGLOBAL: {
                /* Wren风格优化：Bx现在是全局变量的固定索引，而非常量索引 */
                int a = GETARG_A(inst);
                int global_index = GETARG_Bx(inst);
                
                /* 直接从数组读取全局变量（O(1)） */
                if (global_index < vm->global_count) {
                    R(a) = vm->globals_array[global_index];
                } else {
                    R(a) = xr_null();  /* 未定义的全局变量 */
                }
                break;
            }
            
            case OP_SETGLOBAL: {
                /* Wren风格优化：Bx现在是全局变量的固定索引，而非常量索引 */
                int a = GETARG_A(inst);
                int global_index = GETARG_Bx(inst);
                
                /* 直接写入数组（O(1)） */
                vm->globals_array[global_index] = R(a);
                
                /* 更新全局变量计数 */
                if (global_index >= vm->global_count) {
                    vm->global_count = global_index + 1;
                }
                
                break;
            }
            
            case OP_CLOSURE: {
                int a = GETARG_A(inst);
                int bx = GETARG_Bx(inst);
                
                /* 安全检查 */
                if (frame->closure == NULL || frame->closure->proto == NULL) {
                    xr_bc_runtime_error(vm, "Null closure or proto in CLOSURE");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (bx >= frame->closure->proto->sizeprotos) {
                    xr_bc_runtime_error(vm, "Proto index %d out of bounds (max %d)", 
                                     bx, frame->closure->proto->sizeprotos);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                /* 获取函数原型 */
                Proto *proto = frame->closure->proto->protos[bx];
                if (proto == NULL) {
                    xr_bc_runtime_error(vm, "Null proto at index %d", bx);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                /* 创建闭包 */
                XrClosure *closure = xr_bc_closure_new(proto);
                
                /* 捕获upvalues（使用Lua风格的capture机制） */
                for (int i = 0; i < proto->sizeupvalues; i++) {
                    UpvalInfo *uvinfo = &proto->upvalues[i];
                    if (uvinfo->is_local) {
                        /* 捕获局部变量（使用capture_upvalue来重用已有的upvalue） */
                        closure->upvalues[i] = capture_upvalue(vm, &R(uvinfo->index));
                    } else {
                        /* 继承外层upvalue */
                        closure->upvalues[i] = frame->closure->upvalues[uvinfo->index];
                    }
                }
                
                /* 存储闭包（使用正确的闭包值表示） */
                R(a) = xr_value_from_closure(closure);
                break;
            }
            
            case OP_GETUPVAL: {
                /* R[A] = UpValue[B] */
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                
                /* 安全检查 */
                if (frame->closure == NULL) {
                    xr_bc_runtime_error(vm, "Null closure in GETUPVAL");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (b >= frame->closure->upvalue_count) {
                    xr_bc_runtime_error(vm, "Upvalue index %d out of bounds (max %d)", 
                                     b, frame->closure->upvalue_count);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                /* 从当前闭包的upvalue数组中获取值 */
                XrUpvalue *upvalue = frame->closure->upvalues[b];
                if (upvalue == NULL) {
                    xr_bc_runtime_error(vm, "Null upvalue at index %d", b);
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (upvalue->location == NULL) {
                    xr_bc_runtime_error(vm, "Null upvalue location at index %d", b);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                R(a) = *upvalue->location;
                break;
            }
            
            case OP_SETUPVAL: {
                /* UpValue[B] = R[A] */
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                
                /* 安全检查 */
                if (frame->closure == NULL) {
                    xr_bc_runtime_error(vm, "Null closure in SETUPVAL");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (b >= frame->closure->upvalue_count) {
                    xr_bc_runtime_error(vm, "Upvalue index %d out of bounds", b);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                /* 设置upvalue的值 */
                XrUpvalue *upvalue = frame->closure->upvalues[b];
                if (upvalue == NULL) {
                    xr_bc_runtime_error(vm, "Null upvalue at index %d", b);
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (upvalue->location == NULL) {
                    xr_bc_runtime_error(vm, "Null upvalue location at index %d", b);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                *upvalue->location = R(a);
                break;
            }
            
            case OP_CLOSE: {
                /* close upvalues >= R[A] */
                int a = GETARG_A(inst);
                xr_bc_close_upvalues(vm, &R(a));
                break;
            }
            
            case OP_PRINT: {
                /* print(R[A]) - 打印寄存器值 */
                int a = GETARG_A(inst);
                xr_print_value(R(a));
                printf("\n");
                break;
            }
            
            case OP_NOP: {
                /* NOP - 无操作（优化器生成） */
                break;
            }
            
            case OP_NEWTABLE: {
                /* R[A] = {} - 创建新数组/表 */
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);  /* 数组大小提示 */
                
                /* 创建数组 */
                XrArray *array = (b > 0) ? xr_array_with_capacity(b) : xr_array_new();
                
                /* 存储数组 */
                R(a) = xr_value_from_array(array);
                break;
            }
            
            case OP_GETTABLE: {
                /* R[A] = R[B][R[C]] - 获取表元素 */
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int c = GETARG_C(inst);
                
                /* 获取数组对象 */
                XrValue table_val = R(b);
                if (!xr_isarray(table_val)) {
                    xr_bc_runtime_error(vm, "Attempt to index a non-array value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                XrArray *array = xr_to_array(table_val);
                
                /* 获取索引 */
                XrValue index_val = R(c);
                if (!xr_isint(index_val)) {
                    xr_bc_runtime_error(vm, "Array index must be an integer");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                int index = (int)xr_toint(index_val);
                R(a) = xr_array_get(array, index);
                break;
            }
            
            case OP_SETTABLE: {
                /* R[A][R[B]] = R[C] - 设置表元素 */
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int c = GETARG_C(inst);
                
                /* 获取数组对象 */
                XrValue table_val = R(a);
                if (!xr_isarray(table_val)) {
                    xr_bc_runtime_error(vm, "Attempt to index a non-array value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                XrArray *array = xr_to_array(table_val);
                
                /* 获取索引 */
                XrValue index_val = R(b);
                if (!xr_isint(index_val)) {
                    xr_bc_runtime_error(vm, "Array index must be an integer");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                int index = (int)xr_toint(index_val);
                xr_array_set(array, index, R(c));
                break;
            }
            
            case OP_SETLIST: {
                /* R[A][i] = R[A+i], 1 <= i <= B - 批量设置数组元素 */
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                
                /* 获取数组对象 */
                XrValue array_val = R(a);
                if (!xr_isarray(array_val)) {
                    xr_bc_runtime_error(vm, "SETLIST requires an array");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                XrArray *array = xr_to_array(array_val);
                
                /* 批量设置元素（xr_array_set会自动扩展数组） */
                for (int i = 1; i <= b; i++) {
                    xr_array_set(array, i - 1, R(a + i));
                }
                break;
            }
            
            case OP_CALL: {
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
                        xr_bc_runtime_error(vm, "C function '%s' failed", cfunc->name);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    /* 返回值放在R[a]（函数位置） */
                    R(a) = result;
                    
                    /* C函数调用完成，继续执行 */
                    break;
                }
                
                /* Xray闭包：原有路径 */
                if (!xr_isfunction(func_val)) {
                    xr_bc_runtime_error(vm, "Attempt to call a non-function value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                /* 从对象指针获取闭包 */
                XrClosure *closure = xr_value_to_closure(func_val);
                
                /* 快速路径：参数数量匹配（绝大多数情况） */
                if (likely(nargs == closure->proto->numparams)) {
                    /* 检查栈空间 */
                    if (unlikely(vm->frame_count >= FRAMES_MAX)) {
                        xr_bc_runtime_error(vm, "Stack overflow");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    /* 创建新的调用帧 */
                    BcCallFrame *new_frame = &vm->frames[vm->frame_count++];
                    new_frame->closure = closure;
                    new_frame->pc = closure->proto->code;
                    new_frame->base = frame->base + a + 1;  /* 参数从R[a+1]开始 */
                    
                    /* ⭐ Phase 1优化：直接跳转到startfunc
                    ** 避免break的循环开销，直接开始执行新函数
                    */
                    frame = new_frame;
                    goto startfunc;
                } else {
                    /* 慢速路径：参数数量不匹配（错误情况） */
                    xr_bc_runtime_error(vm, "Expected %d arguments but got %d",
                                     closure->proto->numparams, nargs);
                    return INTERPRET_RUNTIME_ERROR;
                }
            }
            
            case OP_CALLSELF: {
                /* ⭐ v0.16.0优化：递归调用自己，无需GETGLOBAL */
                TRACE_EXECUTION();
                int a = GETARG_A(inst);
                int nargs = GETARG_B(inst);
                
                /* 被调用的函数就是当前函数 */
                XrClosure *closure = frame->closure;
                
                /* 快速路径：参数数量匹配（绝大多数情况） */
                if (likely(nargs == closure->proto->numparams)) {
                    /* 检查栈空间 */
                    if (unlikely(vm->frame_count >= FRAMES_MAX)) {
                        xr_bc_runtime_error(vm, "Stack overflow");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    /* 创建新的调用帧 */
                    BcCallFrame *new_frame = &vm->frames[vm->frame_count++];
                    new_frame->closure = closure;  /* 使用相同的closure */
                    new_frame->pc = closure->proto->code;
                    new_frame->base = frame->base + a + 1;  /* 参数从R[a+1]开始 */
                    
                    /* 直接跳转到startfunc */
                    frame = new_frame;
                    goto startfunc;
                } else {
                    /* 参数数量不匹配 */
                    xr_bc_runtime_error(vm, "Expected %d arguments but got %d",
                                     closure->proto->numparams, nargs);
                    return INTERPRET_RUNTIME_ERROR;
                }
            }
            
            case OP_TAILCALL: {
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
                    xr_bc_runtime_error(vm, "Attempt to call a non-function value");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                XrClosure *new_closure = xr_value_to_closure(func_val);
                
                /* 参数数量检查 */
                if (unlikely(nargs != new_closure->proto->numparams)) {
                    xr_bc_runtime_error(vm, "Expected %d arguments but got %d",
                                     new_closure->proto->numparams, nargs);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                /* ⭐ 诊断：检查栈空间 */
                size_t current_stack_offset = frame->base - vm->stack;
                size_t required_stack_size = current_stack_offset + new_closure->proto->maxstacksize;
                
                if (required_stack_size > STACK_MAX) {
                    fprintf(stderr, "DEBUG: Stack overflow detected!\n");
                    fprintf(stderr, "  current offset: %zu\n", current_stack_offset);
                    fprintf(stderr, "  maxstacksize: %d\n", new_closure->proto->maxstacksize);
                    fprintf(stderr, "  required: %zu, max: %d\n", required_stack_size, STACK_MAX);
                    fprintf(stderr, "  frame_count: %d\n", vm->frame_count);
                    xr_bc_runtime_error(vm, "Stack overflow in tail call");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                /* ⭐ 关键步骤1: 关闭当前frame的upvalues */
                xr_bc_close_upvalues(vm, frame->base);
                
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
                vm->stack_top = frame->base + new_closure->proto->maxstacksize;
                
                /* ⭐ 关键步骤5: 直接跳转到startfunc开始执行
                ** 不增加调用深度，实现真正的尾调用优化
                */
                goto startfunc;
            }
            
            case OP_RETURN: {
                TRACE_EXECUTION();
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                
                /* 获取返回值 */
                XrValue result = (b > 0) ? R(a) : xr_null();
                
                /* 关闭upvalues */
                xr_bc_close_upvalues(vm, frame->base);
                
                /* 弹出调用帧 */
                vm->frame_count--;
                
                if (vm->frame_count == 0) {
                    /* 顶层脚本返回 */
                    vm->stack_top = vm->stack;
                    return INTERPRET_OK;
                }
                
                /* 计算返回位置（调用者的寄存器） */
                /* 假设调用是 CALL R[x] ... */
                /* 返回值应该放在R[x]的位置 */
                XrValue *return_slot = frame->base - 1;  /* 函数自身的位置 */
                
                /* 恢复调用者frame */
                frame = &vm->frames[vm->frame_count - 1];
                
                /* 将返回值放到正确位置 */
                *return_slot = result;
                
                /* 更新栈顶 */
                vm->stack_top = return_slot + 1;
                
                /* ⭐ Phase 1优化：返回后直接跳转到startfunc继续执行
                ** 避免break的循环开销
                */
                goto startfunc;
            }
            
            /* === OOP指令（v0.19.0新增）=== */
            
            case OP_CLASS: {
                /* R[A] = new Class(name=K[Bx]) */
                TRACE_EXECUTION();
                int a = GETARG_A(inst);
                int bx = GETARG_Bx(inst);
                XrValue name_val = K(bx);
                
                /* 创建类对象 */
                if (!xr_isstring(name_val)) {
                    xr_bc_runtime_error(vm, "Class name must be a string");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                XrString *class_name = xr_tostring(name_val);
                XrClass *cls = xr_class_new(NULL, class_name->chars, NULL);
                R(a) = xr_value_from_class(cls);
                break;
            }
            
            case OP_ADDFIELD: {
                /* R[A].add_field(K[B], K[C]) - 添加字段定义 */
                TRACE_EXECUTION();
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int c = GETARG_C(inst);
                
                XrValue class_val = R(a);
                XrValue field_name_val = K(b);
                
                if (!xr_value_is_class(class_val)) {
                    xr_bc_runtime_error(vm, "OP_ADDFIELD: not a class");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if (!xr_isstring(field_name_val)) {
                    xr_bc_runtime_error(vm, "Field name must be a string");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                XrClass *cls = xr_value_to_class(class_val);
                XrString *field_name = xr_tostring(field_name_val);
                
                /* 类型名（可选，c=0表示无类型）*/
                XrTypeInfo *type_info = NULL;
                if (c > 0) {
                    XrValue type_name_val = K(c);
                    if (xr_isstring(type_name_val)) {
                        XrString *type_name = xr_tostring(type_name_val);
                        /* TODO: 将类型名转换为XrTypeInfo */
                        (void)type_name;  /* 暂时不用 */
                    }
                }
                
                /* 添加字段到类 */
                xr_class_add_field(cls, field_name->chars, type_info);
                
                break;
            }
            
            case OP_METHOD: {
                /* R[A][K[B] or Symbol[B]] = R[C] - 给类添加方法 */
                /* v0.20.0: 支持Symbol模式 */
                TRACE_EXECUTION();
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int c = GETARG_C(inst);
                
                XrValue class_val = R(a);
                XrValue closure_val = R(c);
                
                if (!xr_value_is_class(class_val)) {
                    xr_bc_runtime_error(vm, "OP_METHOD: not a class");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if (!xr_isfunction(closure_val)) {
                    xr_bc_runtime_error(vm, "Method value must be a function");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                XrClass *cls = xr_value_to_class(class_val);
                XrClosure *closure = xr_value_to_closure(closure_val);
                
                /* v0.20.0: B参数现在是symbol（整数）*/
                int method_symbol = b;
                
                /* 从全局Symbol表获取方法名（用于创建XrMethod）*/
                const char *method_name = symbol_get_name(global_method_symbols, method_symbol);
                if (method_name == NULL) {
                    xr_bc_runtime_error(vm, "Invalid method symbol: %d", method_symbol);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                /* 创建XrFunction对象（从闭包） */
                XrFunction *func = closure->proto;
                
                /* 创建方法对象并通过symbol添加到类（高性能）*/
                XrMethod *method = xr_method_new(NULL, method_name, func, false);
                xr_class_add_method_by_symbol(cls, method_symbol, method);  /* ⭐ 使用by_symbol */
                break;
            }
            
            case OP_INVOKE: {
                /* R[A] = R[A]:Symbol[B](R[A+1]..R[A+C]) - 方法调用 */
                /* v0.20.0: B参数现在是symbol，不再是常量索引 */
                TRACE_EXECUTION();
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int nargs = GETARG_C(inst);
                
                XrValue receiver = R(a);
                
                /* v0.20.0: B参数现在是symbol（整数） */
                int method_symbol = b;
                
                /* 从Symbol表获取方法名（用于查找）*/
                const char *method_name_chars = symbol_get_name(global_method_symbols, method_symbol);
                if (method_name_chars == NULL) {
                    xr_bc_runtime_error(vm, "Invalid method symbol: %d", method_symbol);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                /* 检查是否为类（用于new操作） */
                if (xr_value_is_class(receiver)) {
                    /* 调用构造函数：创建实例 */
                    XrClass *cls = xr_value_to_class(receiver);
                    
                    if (strcmp(method_name_chars, "constructor") == 0) {
                        /* 创建实例 */
                        XrInstance *inst = xr_instance_new(NULL, cls);
                        XrValue inst_val = xr_value_from_instance(inst);
                        
                        /* v0.20.0: 通过symbol查找构造函数（高性能）*/
                        XrMethod *ctor = xr_class_lookup_method_by_symbol(cls, method_symbol);
                        if (ctor != NULL && ctor->func != NULL) {
                            /* 获取方法的Proto（注意：func实际是Proto*） */
                            Proto *proto = (Proto*)ctor->func;
                            
                            /* 检查参数数量（构造函数没有显式this参数） */
                            /* constructor(x, y) 编译时第一个参数是隐式的this */
                            /* 但调用时nargs不包含this */
                            if (nargs + 1 != proto->numparams) {  /* +1因为编译时添加了this */
                                xr_bc_runtime_error(vm, "Constructor expects %d arguments but got %d",
                                                 proto->numparams - 1, nargs);
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            
                            /* 检查栈空间 */
                            if (vm->frame_count >= FRAMES_MAX) {
                                xr_bc_runtime_error(vm, "Stack overflow");
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            
                            /* 创建堆上的闭包对象 */
                            XrClosure *closure = (XrClosure*)gc_alloc(sizeof(XrClosure), OBJ_CLOSURE);
                            closure->header.type = XR_TFUNCTION;
                            closure->header.next = vm->objects;
                            closure->header.marked = false;
                            closure->proto = proto;
                            closure->upvalue_count = 0;
                            closure->upvalues = NULL;
                            vm->objects = (XrObject*)closure;
                            
                            /* 将this放到参数位置的第一个 */
                            /* 参数布局：R[a+1] = this, R[a+2] = arg1, R[a+3] = arg2, ... */
                            /* 需要将参数右移一位，为this腾出空间 */
                            for (int i = nargs; i > 0; i--) {
                                R(a + 1 + i) = R(a + 1 + i - 1);
                            }
                            R(a + 1) = inst_val;  /* this */
                            
                            /* 创建新的调用帧 */
                            BcCallFrame *new_frame = &vm->frames[vm->frame_count++];
                            new_frame->closure = closure;
                            new_frame->pc = proto->code;
                            new_frame->base = frame->base + a + 1;  /* 参数从R[a+1]开始 */
                            
                            /* 跳转到新函数 */
                            frame = new_frame;
                            goto startfunc;
                        }
                        
                        /* 没有构造函数或执行完毕，返回实例 */
                        R(a) = inst_val;
                    } else {
                        xr_bc_runtime_error(vm, "Cannot call method '%s' on class", method_name_chars);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                } else if (xr_value_is_instance(receiver)) {
                    /* 调用实例方法 */
                    XrInstance *inst = xr_value_to_instance(receiver);
                    
                /* v0.20.0: 通过symbol查找方法（高性能O(1)）*/
                XrMethod *method = xr_class_lookup_method_by_symbol(inst->klass, method_symbol);
                    if (method == NULL || method->func == NULL) {
                        xr_bc_runtime_error(vm, "Method '%s' not found", method_name_chars);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    /* 获取方法的Proto（注意：func实际是Proto*） */
                    Proto *proto = (Proto*)method->func;
                    
                    /* 检查参数数量 */
                    if (nargs + 1 != proto->numparams) {  /* +1因为编译时添加了this */
                        xr_bc_runtime_error(vm, "Method '%s' expects %d arguments but got %d",
                                         method_name_chars, proto->numparams - 1, nargs);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    /* 检查栈空间 */
                    if (vm->frame_count >= FRAMES_MAX) {
                        xr_bc_runtime_error(vm, "Stack overflow");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    /* 创建堆上的闭包对象 */
                    XrClosure *closure = (XrClosure*)gc_alloc(sizeof(XrClosure), OBJ_CLOSURE);
                    closure->header.type = XR_TFUNCTION;
                    closure->header.next = vm->objects;
                    closure->header.marked = false;
                    closure->proto = proto;
                    closure->upvalue_count = 0;
                    closure->upvalues = NULL;
                    vm->objects = (XrObject*)closure;
                    
                    /* 将this放到参数位置的第一个 */
                    for (int i = nargs; i > 0; i--) {
                        R(a + 1 + i) = R(a + 1 + i - 1);
                    }
                    R(a + 1) = receiver;  /* this */
                    
                    /* 创建新的调用帧 */
                    BcCallFrame *new_frame = &vm->frames[vm->frame_count++];
                    new_frame->closure = closure;
                    new_frame->pc = proto->code;
                    new_frame->base = frame->base + a + 1;
                    
                    /* 跳转到新函数 */
                    frame = new_frame;
                    goto startfunc;
                } else {
                    xr_bc_runtime_error(vm, "INVOKE: receiver must be a class or instance");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            
            case OP_GETPROP: {
                /* R[A] = R[B].K[C] */
                TRACE_EXECUTION();
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int c = GETARG_C(inst);
                
                XrValue obj = R(b);
                XrValue prop_name_val = K(c);
                
                if (!xr_value_is_instance(obj)) {
                    xr_bc_runtime_error(vm, "Only instances have properties");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if (!xr_isstring(prop_name_val)) {
                    xr_bc_runtime_error(vm, "Property name must be a string");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                XrInstance *inst = xr_value_to_instance(obj);
                XrString *prop_name = xr_tostring(prop_name_val);
                
                /* 验证字段是否在类中声明（仅当类有字段定义时）*/
                if (inst->klass->field_count > 0) {
                    int field_index = xr_class_find_field_index(inst->klass, prop_name->chars);
                    if (field_index < 0) {
                        xr_bc_runtime_error(vm, "字段 '%s' 未在类 '%s' 中声明",
                                         prop_name->chars, inst->klass->name);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                }
                
                R(a) = xr_instance_get_field(inst, prop_name->chars);
                break;
            }
            
            case OP_SETPROP: {
                /* R[A].K[B] = R[C] */
                TRACE_EXECUTION();
                int a = GETARG_A(inst);
                int b = GETARG_B(inst);
                int c = GETARG_C(inst);
                
                XrValue obj = R(a);
                XrValue prop_name_val = K(b);
                XrValue value = R(c);
                
                if (!xr_value_is_instance(obj)) {
                    xr_bc_runtime_error(vm, "Only instances have properties");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if (!xr_isstring(prop_name_val)) {
                    xr_bc_runtime_error(vm, "Property name must be a string");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                XrInstance *inst = xr_value_to_instance(obj);
                XrString *prop_name = xr_tostring(prop_name_val);
                
                /* 验证字段是否在类中声明（仅当类有字段定义时）*/
                if (inst->klass->field_count > 0) {
                    int field_index = xr_class_find_field_index(inst->klass, prop_name->chars);
                    if (field_index < 0) {
                        xr_bc_runtime_error(vm, "字段 '%s' 未在类 '%s' 中声明",
                                         prop_name->chars, inst->klass->name);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                }
                
                xr_instance_set_field(inst, prop_name->chars, value);
                break;
            }
            
            default:
                /* 未知或未实现的指令 */
                xr_bc_runtime_error(vm, "Unknown opcode %d", op);
                return INTERPRET_RUNTIME_ERROR;
        }
        
        /* 读取下一条指令 */
        inst = READ_INSTRUCTION();
    }
}

/* ========== C代码调用闭包API ========== */

/*
** 从C代码调用Xray闭包
** 用于高阶函数（map/filter/reduce等）
*/
XrValue xr_bc_call_closure(VM *vm, XrClosure *closure, XrValue *args, int nargs) {
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
    XrValue *saved_stack_top = vm->stack_top;
    int saved_frame_count = vm->frame_count;
    
    /* 创建新的调用帧 */
    if (vm->frame_count >= FRAMES_MAX) {
        fprintf(stderr, "Stack overflow in callback\n");
        return xr_null();
    }
    
    BcCallFrame *frame = &vm->frames[vm->frame_count++];
    frame->closure = closure;
    frame->pc = closure->proto->code;
    frame->base = vm->stack_top;  /* 新frame从当前栈顶开始 */
    
    /* 复制参数到栈上 */
    for (int i = 0; i < nargs; i++) {
        vm->stack_top[i] = args[i];
    }
    
    /* 更新栈顶 */
    vm->stack_top = frame->base + closure->proto->maxstacksize;
    
    /* 执行字节码 */
    InterpretResult result = run(vm);
    
    /* 获取返回值（在frame->base - 1位置）*/
    XrValue return_value = (vm->frame_count == saved_frame_count) ? 
                           *(frame->base - 1) : xr_null();
    
    /* 恢复栈状态 */
    vm->stack_top = saved_stack_top;
    vm->frame_count = saved_frame_count;
    
    return return_value;
}

/* ========== 解释执行API ========== */

/*
** 执行函数原型
*/
InterpretResult xr_bc_interpret_proto(VM *vm, Proto *proto) {
    /* 创建顶层闭包 */
    XrClosure *closure = xr_bc_closure_new(proto);
    if (closure == NULL) {
        return INTERPRET_RUNTIME_ERROR;
    }
    
    /* 不需要压栈 - 直接创建调用帧 */
    vm->stack_top = vm->stack;
    
    /* 创建调用帧 */
    BcCallFrame *frame = &vm->frames[vm->frame_count++];
    frame->closure = closure;
    frame->pc = proto->code;
    frame->base = vm->stack;
    
    /* 执行 */
    return run(vm);
}

/*
** 执行源代码
*/
InterpretResult xr_bc_interpret(const char *source) {
    /* 编译流程：通过xr_compile已实现 */
    /* 目前需要xcompiler模块 */
    fprintf(stderr, "Compiler not yet implemented\n");
    return INTERPRET_COMPILE_ERROR;
}


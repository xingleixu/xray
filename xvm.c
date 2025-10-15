/*
** xvm.c
** Xray 寄存器虚拟机实现
*/

#include "xvm.h"
#include "xdebug.h"
#include "xmem.h"
#include "xstring.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

/* 全局VM实例 */
VM vm;

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
    
    /* 初始化全局变量表 */
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

/* ========== VM执行循环 ========== */

/*
** VM执行循环
*/
static InterpretResult run(void) {
    BcCallFrame *frame = &vm.frames[vm.frame_count - 1];
    
    /* 主循环 */
    for (;;) {
        /* 调试跟踪 */
        if (vm.trace_execution) {
            /* 打印栈状态 */
            printf("          ");
            for (XrValue *slot = vm.stack; slot < vm.stack_top; slot++) {
                printf("[ ");
                xr_print_value(*slot);
                printf(" ]");
            }
            printf("\n");
            
            /* 反汇编当前指令 */
            xr_disassemble_instruction(frame->closure->proto,
                                      (int)(frame->pc - frame->closure->proto->code));
        }
        
        /* 读取指令 */
        Instruction inst = READ_INSTRUCTION();
        OpCode op = GET_OPCODE(inst);
        
        /* 执行指令 */
        switch (op) {
            case OP_MOVE: {
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
                
                /* TODO: 完善类型检查和转换 */
                if (xr_isint(R(b)) && xr_isint(R(c))) {
                    R(a) = xr_int(xr_toint(R(b)) + xr_toint(R(c)));
                } else {
                    /* 转换为浮点数运算 */
                    double nb = xr_isint(R(b)) ? (double)xr_toint(R(b)) : xr_tofloat(R(b));
                    double nc = xr_isint(R(c)) ? (double)xr_toint(R(c)) : xr_tofloat(R(c));
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
            
            case OP_DIV: {
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
                break;
            }
            
            case OP_MOD: {
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
                    xr_bc_runtime_error("Operand must be a number");
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
                int a = GETARG_A(inst);
                int bx = GETARG_Bx(inst);
                XrValue key = K(bx);
                
                /* 从全局变量表查找 */
                if (vm.globals != NULL && xr_isstring(key)) {
                    XrString *name = xr_tostring(key);
                    XrValue *value_ptr = (XrValue *)xr_hashmap_get(vm.globals, name->chars);
                    if (value_ptr != NULL) {
                        R(a) = *value_ptr;
                    } else {
                        R(a) = xr_null();  /* 未定义的全局变量 */
                    }
                } else {
                    R(a) = xr_null();
                }
                break;
            }
            
            case OP_SETGLOBAL: {
                int a = GETARG_A(inst);
                int bx = GETARG_Bx(inst);
                XrValue key = K(bx);
                
                /* 设置全局变量 */
                if (vm.globals != NULL && xr_isstring(key)) {
                    XrString *name = xr_tostring(key);
                    XrValue *value_copy = (XrValue *)xr_malloc(sizeof(XrValue));
                    *value_copy = R(a);
                    xr_hashmap_set(vm.globals, name->chars, value_copy);
                }
                break;
            }
            
            case OP_CLOSURE: {
                int a = GETARG_A(inst);
                int bx = GETARG_Bx(inst);
                
                /* 获取函数原型 */
                Proto *proto = frame->closure->proto->protos[bx];
                
                /* 创建闭包 */
                XrClosure *closure = xr_bc_closure_new(proto);
                
                /* 捕获upvalues（如果有） */
                for (int i = 0; i < proto->sizeupvalues; i++) {
                    UpvalInfo *uvinfo = &proto->upvalues[i];
                    if (uvinfo->is_local) {
                        /* 捕获局部变量 */
                        closure->upvalues[i] = xr_bc_upvalue_new(&R(uvinfo->index));
                    } else {
                        /* 继承外层upvalue */
                        closure->upvalues[i] = frame->closure->upvalues[uvinfo->index];
                    }
                }
                
                /* 存储闭包（暂时使用临时方案） */
                /* TODO: 实现正确的闭包值表示 */
                R(a) = xr_int((xr_Integer)(uintptr_t)closure);  /* 临时方案 */
                break;
            }
            
            case OP_CALL: {
                int a = GETARG_A(inst);
                int nargs = GETARG_B(inst);
                
                /* 获取被调用的函数（暂时从寄存器读取） */
                /* TODO: 实现正确的函数值检查 */
                XrClosure *closure = (XrClosure *)(uintptr_t)xr_toint(R(a));
                
                /* 检查参数数量 */
                if (nargs != closure->proto->numparams) {
                    xr_bc_runtime_error("Expected %d arguments but got %d",
                                     closure->proto->numparams, nargs);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                /* 创建新的调用帧 */
                if (vm.frame_count >= FRAMES_MAX) {
                    xr_bc_runtime_error("Stack overflow");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                BcCallFrame *new_frame = &vm.frames[vm.frame_count++];
                new_frame->closure = closure;
                new_frame->pc = closure->proto->code;
                new_frame->base = frame->base + a + 1;  /* 参数从R[a+1]开始 */
                
                /* 更新frame指针 */
                frame = new_frame;
                break;
            }
            
            case OP_RETURN: {
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
                
                break;
            }
            
            default:
                xr_bc_runtime_error("Unknown opcode %d", op);
                return INTERPRET_RUNTIME_ERROR;
        }
    }
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
    
    /* 压入栈 */
    vm.stack[0] = xr_value_from_class((void*)closure);  /* TODO: 正确的值类型 */
    vm.stack_top = vm.stack + 1;
    
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


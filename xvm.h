/*
** xvm.h
** Xray 寄存器虚拟机
** 
** v0.13.0: 寄存器based VM，参考Lua设计
** 创建日期: 2025-10-15
*/

#ifndef xvm_h
#define xvm_h

#include "xchunk.h"
#include "xvalue.h"
#include "xhashmap.h"
#include <stdbool.h>

/* ========== 常量定义 ========== */

#define FRAMES_MAX 64               /* 最大调用帧数量 */
#define STACK_MAX (FRAMES_MAX * 256) /* 最大栈大小（寄存器数量） */

/* ========== 闭包对象 ========== */

/* Upvalue对象（开放和关闭状态） */
typedef struct XrUpvalue {
    XrObject header;            /* GC对象头 */
    XrValue *location;          /* 指向值的位置（栈上或closed字段） */
    XrValue closed;             /* 关闭后的值 */
    struct XrUpvalue *next;     /* 开放upvalue链表 */
} XrUpvalue;

/* 闭包对象 */
typedef struct {
    XrObject header;            /* GC对象头 */
    Proto *proto;               /* 函数原型 */
    XrUpvalue **upvalues;       /* upvalue数组 */
    int upvalue_count;          /* upvalue数量 */
} XrClosure;

/* ========== 调用帧 ========== */

/* 调用帧（函数调用栈帧） */
typedef struct {
    XrClosure *closure;         /* 当前闭包 */
    Instruction *pc;            /* 程序计数器 */
    XrValue *base;              /* 寄存器基址（重要！） */
} BcCallFrame;

/* ========== 虚拟机状态 ========== */

/* VM状态 */
typedef struct {
    /* 寄存器栈 */
    XrValue stack[STACK_MAX];   /* 寄存器栈 */
    XrValue *stack_top;         /* 栈顶指针 */
    
    /* 调用帧 */
    BcCallFrame frames[FRAMES_MAX]; /* 调用帧栈 */
    int frame_count;            /* 当前帧数量 */
    
    /* Upvalue链表 */
    XrUpvalue *open_upvalues;   /* 开放的upvalue链表 */
    
    /* 全局变量 */
    XrHashMap *globals;         /* 全局变量表 */
    
    /* 字符串驻留表 */
    XrHashMap *strings;         /* 字符串驻留表 */
    
    /* GC对象链表 */
    XrObject *objects;          /* 所有对象的链表 */
    size_t bytes_allocated;     /* 已分配字节数 */
    size_t next_gc;             /* 下次GC阈值 */
    
    /* 调试选项 */
    bool trace_execution;       /* 是否跟踪执行 */
} VM;

/* ========== 执行结果 ========== */

typedef enum {
    INTERPRET_OK,               /* 执行成功 */
    INTERPRET_COMPILE_ERROR,    /* 编译错误 */
    INTERPRET_RUNTIME_ERROR     /* 运行时错误 */
} InterpretResult;

/* ========== 全局VM实例 ========== */

extern VM vm;

/* ========== VM API ========== */

/*
** 初始化虚拟机
*/
void xr_bc_vm_init(void);

/*
** 释放虚拟机
*/
void xr_bc_vm_free(void);

/*
** 执行源代码
** @param source 源代码字符串
** @return 执行结果
*/
InterpretResult xr_bc_interpret(const char *source);

/*
** 执行函数原型
** @param proto 函数原型
** @return 执行结果
*/
InterpretResult xr_bc_interpret_proto(Proto *proto);

/* ========== 闭包API ========== */

/*
** 创建闭包对象
*/
XrClosure *xr_bc_closure_new(Proto *proto);

/*
** 释放闭包对象
*/
void xr_bc_closure_free(XrClosure *closure);

/* ========== Upvalue API ========== */

/*
** 创建upvalue对象
*/
XrUpvalue *xr_bc_upvalue_new(XrValue *location);

/*
** 释放upvalue对象
*/
void xr_bc_upvalue_free(XrUpvalue *upvalue);

/*
** 关闭upvalue（从栈转移到堆）
*/
void xr_bc_close_upvalues(XrValue *last);

/* ========== 运行时错误 ========== */

/*
** 报告运行时错误
*/
void xr_bc_runtime_error(const char *format, ...);

#endif /* xvm_h */


/*
** xstate.h
** Xray 状态管理
** 定义和管理 Xray 虚拟机的全局状态
*/

#ifndef xstate_h
#define xstate_h

#include "xray.h"
#include "xvalue.h"
#include "xscope.h"  /* XSymbolTable 在 xscope.h 中定义 */

/*
** 调用帧
** 存储单次函数调用的上下文信息
*/
typedef struct CallFrame {
    XrFunction *function;              /* 当前执行的函数 */
    XSymbolTable *local_symbols;       /* 局部变量符号表 */
    struct CallFrame *prev;            /* 上一帧（形成栈） */
    int line;                          /* 当前行号（用于错误报告） */
} CallFrame;

/*
** 调用栈
** 管理函数调用的栈结构
*/
typedef struct {
    CallFrame *top;                    /* 栈顶帧 */
    int depth;                         /* 当前深度 */
    int max_depth;                     /* 最大深度限制 */
} CallStack;

/*
** Xray 状态结构
** 包含虚拟机运行所需的所有状态信息
*/
struct XrayState {
    void *userdata;                    /* 用户数据指针 */
    CallStack *call_stack;             /* 调用栈 */
    void *type_aliases;                /* 类型别名表（TypeAliasTable*，避免循环依赖） */
};

/* ========== 调用栈操作函数 ========== */

/* 创建新的调用栈 */
CallStack *xr_callstack_new(int max_depth);

/* 释放调用栈 */
void xr_callstack_free(CallStack *stack);

/* 压入新的调用帧 */
int xr_callstack_push(CallStack *stack, XrFunction *func, 
                      XSymbolTable *symbols, int line);

/* 弹出调用帧 */
void xr_callstack_pop(CallStack *stack);

/* 获取栈顶帧 */
CallFrame *xr_callstack_top(CallStack *stack);

/* 获取当前调用深度 */
int xr_callstack_depth(CallStack *stack);

/* ========== 状态机操作函数 ========== */

/*
** 创建新的Xray状态机
** 
** @return 新创建的状态机
*/
XrayState* xr_state_new(void);

/*
** 释放Xray状态机
** 
** @param X 状态机对象
*/
void xr_state_free(XrayState *X);

#endif /* xstate_h */


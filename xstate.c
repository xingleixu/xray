/*
** xstate.c
** Xray 状态管理实现
*/

#include <stdlib.h>
#include "xstate.h"

/*
** 创建新的 Xray 状态
** 返回：新创建的状态对象，失败返回 NULL
*/
XrayState *xray_newstate(void) {
    XrayState *X = (XrayState *)malloc(sizeof(XrayState));
    if (X == NULL) {
        return NULL;
    }
    X->userdata = NULL;
    X->call_stack = NULL;
    X->type_aliases = NULL;  /* 类型别名表（延迟初始化） */
    return X;
}

/*
** 关闭并释放 Xray 状态
** 参数：
**   X - 要释放的状态对象
*/
void xray_close(XrayState *X) {
    if (X != NULL) {
        free(X);
    }
}

/*
** 执行文件
** 参数：
**   X - Xray 状态
**   filename - 要执行的文件名
** 返回：0 表示成功，非 0 表示错误
*/
int xray_dofile(XrayState *X, const char *filename) {
    /* TODO: 实现文件执行功能 */
    (void)X;
    (void)filename;
    return 0;
}

/*
** 执行字符串代码
** 参数：
**   X - Xray 状态
**   source - 要执行的源代码字符串
** 返回：0 表示成功，非 0 表示错误
*/
int xray_dostring(XrayState *X, const char *source) {
    /* TODO: 实现字符串代码执行功能 */
    (void)X;
    (void)source;
    return 0;
}

/* ========== 调用栈实现 ========== */

/*
** 创建新的调用栈
** max_depth: 最大调用深度限制
** 返回：新创建的调用栈，失败返回 NULL
*/
CallStack *xr_callstack_new(int max_depth) {
    CallStack *stack = (CallStack *)malloc(sizeof(CallStack));
    if (stack == NULL) {
        return NULL;
    }
    
    stack->top = NULL;
    stack->depth = 0;
    stack->max_depth = max_depth;
    
    return stack;
}

/*
** 释放调用栈
** stack: 要释放的调用栈
*/
void xr_callstack_free(CallStack *stack) {
    if (stack == NULL) {
        return;
    }
    
    /* 释放所有调用帧 */
    while (stack->top != NULL) {
        CallFrame *frame = stack->top;
        stack->top = frame->prev;
        free(frame);
    }
    
    free(stack);
}

/*
** 压入新的调用帧
** stack: 调用栈
** func: 函数对象
** symbols: 局部变量符号表
** line: 当前行号
** 返回：1 表示成功，0 表示栈溢出
*/
int xr_callstack_push(CallStack *stack, XrFunction *func,
                      XSymbolTable *symbols, int line) {
    if (stack == NULL) {
        return 0;
    }
    
    /* 检查栈深度 */
    if (stack->depth >= stack->max_depth) {
        return 0;  /* 栈溢出 */
    }
    
    /* 创建新的调用帧 */
    CallFrame *frame = (CallFrame *)malloc(sizeof(CallFrame));
    if (frame == NULL) {
        return 0;
    }
    
    frame->function = func;
    frame->local_symbols = symbols;
    frame->line = line;
    frame->prev = stack->top;
    
    /* 压入栈 */
    stack->top = frame;
    stack->depth++;
    
    return 1;
}

/*
** 弹出调用帧
** stack: 调用栈
*/
void xr_callstack_pop(CallStack *stack) {
    if (stack == NULL || stack->top == NULL) {
        return;
    }
    
    CallFrame *frame = stack->top;
    stack->top = frame->prev;
    stack->depth--;
    
    free(frame);
}

/*
** 获取栈顶帧
** stack: 调用栈
** 返回：栈顶帧，栈为空返回 NULL
*/
CallFrame *xr_callstack_top(CallStack *stack) {
    if (stack == NULL) {
        return NULL;
    }
    return stack->top;
}

/*
** 获取当前调用深度
** stack: 调用栈
** 返回：当前深度
*/
int xr_callstack_depth(CallStack *stack) {
    if (stack == NULL) {
        return 0;
    }
    return stack->depth;
}

/* ========== 状态机操作实现 ========== */

/*
** 创建新的Xray状态机（简化版，用于测试）
*/
XrayState* xr_state_new(void) {
    return xray_newstate();
}

/*
** 释放Xray状态机（简化版，用于测试）
*/
void xr_state_free(XrayState *X) {
    xray_close(X);
}


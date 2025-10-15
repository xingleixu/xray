/*
** xeval.h
** Xray 表达式求值器（树遍历解释器）
** 遍历 AST 并执行表达式计算
*/

#ifndef xeval_h
#define xeval_h

#include "xray.h"
#include "xast.h"
#include "xvalue.h"
#include "xscope.h"

/*
** 循环控制状态
** 用于处理 break 和 continue 语句
*/
typedef enum {
    LOOP_NONE,       /* 正常状态 */
    LOOP_BREAK,      /* break 触发 */
    LOOP_CONTINUE,   /* continue 触发 */
} LoopControlState;

typedef struct {
    LoopControlState state;  /* 当前状态 */
    int loop_depth;          /* 循环深度（用于检查 break/continue 的合法性） */
} LoopControl;

/*
** 返回控制状态
** 用于处理 return 语句
*/
typedef struct {
    int has_returned;        /* 是否执行了 return */
    XrValue return_value;    /* 返回值 */
} ReturnControl;

/*
** 主要求值函数
** 遍历 AST 节点并返回计算结果（内部创建符号表）
*/
XrValue xr_eval(XrayState *X, AstNode *node);

/*
** 使用外部符号表的求值函数（用于 REPL）
** 允许跨多次调用保持变量状态
*/
XrValue xr_eval_with_symbols(XrayState *X, AstNode *node, XSymbolTable *symbols);

/*
** 内部求值函数（v0.12.0: 供OOP模块使用）
** 支持loop和return控制
*/
XrValue xr_eval_internal(XrayState *X, AstNode *node, XSymbolTable *symbols, 
                         LoopControl *loop, ReturnControl *ret);

/* ========== 内部求值函数（供 xeval.c 内部使用）========== */
/* 这些函数现在是 xeval.c 的内部静态函数，不对外暴露 */

/*
** 变量相关求值（内部函数，需要loop参数）
*/
XrValue xr_eval_var_decl(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret);
XrValue xr_eval_variable(XrayState *X, AstNode *node, XSymbolTable *symbols);
XrValue xr_eval_assignment(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret);
XrValue xr_eval_block(XrayState *X, AstNode *node, XSymbolTable *symbols);

/*
** 控制流语句求值（内部函数）
** 这些函数需要 LoopControl 参数来处理 break/continue
*/
XrValue xr_eval_if_stmt(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret);
XrValue xr_eval_while_stmt(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret);
XrValue xr_eval_for_stmt(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret);
XrValue xr_eval_break_stmt(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop);
XrValue xr_eval_continue_stmt(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop);

/*
** 函数相关求值（内部函数）
** 这些函数需要 ReturnControl 参数来处理 return
*/
XrValue xr_eval_function_decl(XrayState *X, AstNode *node, XSymbolTable *symbols);
XrValue xr_eval_function_expr(XrayState *X, AstNode *node, XSymbolTable *symbols);
XrValue xr_eval_call_expr(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret);
XrValue xr_eval_return_stmt(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret);

/*
** 调用函数（供OOP系统使用）
** 
** @param func          函数对象
** @param args          参数数组
** @param arg_count     参数数量
** @param parent_symbols 父符号表
** @return              返回值
*/
XrValue xr_eval_call_function(XrFunction *func, XrValue *args, int arg_count, XSymbolTable *parent_symbols);

/*
** 数组相关求值
*/
XrValue xr_eval_array_literal(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret);
XrValue xr_eval_index_get(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret);
XrValue xr_eval_index_set(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret);
XrValue xr_eval_member_access(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret);
XrValue xr_eval_array_method_call(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret);

/* ========== 运算辅助函数 ========== */

/*
** 算术运算
*/
XrValue xr_eval_add(XrayState *X, XrValue left, XrValue right);
XrValue xr_eval_subtract(XrayState *X, XrValue left, XrValue right);
XrValue xr_eval_multiply(XrayState *X, XrValue left, XrValue right);
XrValue xr_eval_divide(XrayState *X, XrValue left, XrValue right);
XrValue xr_eval_modulo(XrayState *X, XrValue left, XrValue right);

/*
** 比较运算
*/
XrValue xr_eval_equal(XrayState *X, XrValue left, XrValue right);
XrValue xr_eval_not_equal(XrayState *X, XrValue left, XrValue right);
XrValue xr_eval_less(XrayState *X, XrValue left, XrValue right);
XrValue xr_eval_less_equal(XrayState *X, XrValue left, XrValue right);
XrValue xr_eval_greater(XrayState *X, XrValue left, XrValue right);
XrValue xr_eval_greater_equal(XrayState *X, XrValue left, XrValue right);

/*
** 逻辑运算（支持短路求值） - 现在是内部函数
*/
/* 逻辑非仍然是公开的，因为可能被其他地方调用 */
XrValue xr_eval_logical_not(XrayState *X, XrValue operand);

/*
** 一元运算
*/
XrValue xr_eval_negate(XrayState *X, XrValue operand);

/* ========== 类型检查和转换 ========== */

/*
** 类型检查函数
*/
int xr_is_number(XrValue value);
int xr_is_truthy(XrValue value);

/*
** 类型转换函数
*/
xr_Number xr_to_number(XrayState *X, XrValue value);

/*
** 值比较函数
*/
int xr_values_equal(XrValue a, XrValue b);

/*
** 值转字符串（用于打印和错误信息）
*/
const char *xr_value_to_string(XrayState *X, XrValue value);

/* ========== OOP求值函数（v0.12.0新增）========== */

/*
** 求值类声明
*/
XrValue xr_eval_class_decl(XrayState *X, AstNode *node, XSymbolTable *symbols);

/*
** 求值new表达式
*/
XrValue xr_eval_new_expr(XrayState *X, AstNode *node, XSymbolTable *symbols);

/*
** 求值this表达式
*/
XrValue xr_eval_this_expr(XrayState *X, AstNode *node, XSymbolTable *symbols);

/*
** 求值super调用
*/
XrValue xr_eval_super_call(XrayState *X, AstNode *node, XSymbolTable *symbols);

/*
** 求值成员赋值
*/
XrValue xr_eval_member_set(XrayState *X, AstNode *node, XSymbolTable *symbols, 
                           LoopControl *loop, ReturnControl *ret);

/* ========== 错误处理 ========== */

/*
** 运行时错误报告
*/
void xr_runtime_error(XrayState *X, int line, const char *format, ...);

#endif /* xeval_h */

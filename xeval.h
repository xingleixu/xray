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
** 主要求值函数
** 遍历 AST 节点并返回计算结果（内部创建符号表）
*/
XrValue xr_eval(XrayState *X, AstNode *node);

/*
** 使用外部符号表的求值函数（用于 REPL）
** 允许跨多次调用保持变量状态
*/
XrValue xr_eval_with_symbols(XrayState *X, AstNode *node, XSymbolTable *symbols);

/* ========== 内部求值函数（供 xeval.c 内部使用）========== */
/* 这些函数现在是 xeval.c 的内部静态函数，不对外暴露 */

/*
** 变量相关求值
*/
XrValue xr_eval_var_decl(XrayState *X, AstNode *node, XSymbolTable *symbols);
XrValue xr_eval_variable(XrayState *X, AstNode *node, XSymbolTable *symbols);
XrValue xr_eval_assignment(XrayState *X, AstNode *node, XSymbolTable *symbols);
XrValue xr_eval_block(XrayState *X, AstNode *node, XSymbolTable *symbols);

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

/* ========== 错误处理 ========== */

/*
** 运行时错误报告
*/
void xr_runtime_error(XrayState *X, int line, const char *format, ...);

#endif /* xeval_h */

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

/*
** 主要求值函数
** 遍历 AST 节点并返回计算结果
*/
XrValue xr_eval(XrayState *X, AstNode *node);

/* ========== 内部求值函数 ========== */

/*
** 字面量求值
*/
XrValue xr_eval_literal(XrayState *X, AstNode *node);

/*
** 二元运算求值
** 支持算术、比较、逻辑运算，包括短路求值
*/
XrValue xr_eval_binary(XrayState *X, AstNode *node);

/*
** 一元运算求值
*/
XrValue xr_eval_unary(XrayState *X, AstNode *node);

/*
** 分组表达式求值（括号）
*/
XrValue xr_eval_grouping(XrayState *X, AstNode *node);

/*
** 语句求值
*/
XrValue xr_eval_expr_stmt(XrayState *X, AstNode *node);
XrValue xr_eval_print_stmt(XrayState *X, AstNode *node);

/*
** 程序求值（执行多个语句）
*/
XrValue xr_eval_program(XrayState *X, AstNode *node);

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
** 逻辑运算（支持短路求值）
*/
XrValue xr_eval_logical_and(XrayState *X, AstNode *left_node, AstNode *right_node);
XrValue xr_eval_logical_or(XrayState *X, AstNode *left_node, AstNode *right_node);
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

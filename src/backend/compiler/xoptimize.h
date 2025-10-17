/*
** xoptimize.h
** Xray 编译器优化模块
** v0.15.0 - 常量折叠优化
*/

#ifndef XOPTIMIZE_H
#define XOPTIMIZE_H

#include "xast.h"
#include "xvalue.h"
#include "xlex.h"
#include <stdbool.h>

/* ========== 常量折叠优化 ========== */

/*
** 尝试折叠二元运算的常量
** 
** 参数：
**   op     - 运算符类型（TK_PLUS, TK_MINUS等）
**   left   - 左操作数（必须是数字）
**   right  - 右操作数（必须是数字）
**   result - 输出结果
** 
** 返回：
**   true  - 折叠成功，result包含计算结果
**   false - 无法折叠（非数字操作数、除零、NaN等）
** 
** 示例：
**   折叠 "3 + 5" -> 8
**   折叠 "10 * 2" -> 20
**   无法折叠 "1 / 0" -> false（除零）
*/
bool xr_opt_fold_binary(
    TokenType op,
    XrValue left,
    XrValue right,
    XrValue *result
);

/*
** 尝试折叠一元运算的常量
** 
** 参数：
**   op     - 运算符类型（TK_MINUS, TK_NOT）
**   value  - 操作数
**   result - 输出结果
** 
** 返回：
**   true  - 折叠成功
**   false - 无法折叠
** 
** 示例：
**   折叠 "-5" -> -5
**   折叠 "!true" -> false
*/
bool xr_opt_fold_unary(
    TokenType op,
    XrValue value,
    XrValue *result
);

/* ========== 优化统计（调试用） ========== */

typedef struct {
    int fold_count;         /* 折叠成功次数 */
    int fold_binary;        /* 二元运算折叠 */
    int fold_unary;         /* 一元运算折叠 */
} OptStats;

extern OptStats g_opt_stats;

/* 重置统计 */
void xr_opt_reset_stats(void);

/* 打印统计信息 */
void xr_opt_print_stats(void);

#endif /* XOPTIMIZE_H */


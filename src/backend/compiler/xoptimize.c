/*
** xoptimize.c
** Xray 编译器优化实现
** v0.15.0 - 常量折叠优化
*/

#include "xoptimize.h"
#include "xvalue.h"
#include <math.h>
#include <stdio.h>

/* 优化统计 */
OptStats g_opt_stats = {0};

/* ========== 常量折叠实现 ========== */

/*
** 折叠二元运算
** 参考 Lua lcode.c:1371 constfolding()
*/
bool xr_opt_fold_binary(
    TokenType op,
    XrValue left,
    XrValue right,
    XrValue *result
) {
    /* 1. 类型检查：只折叠数字运算 */
    if (!xr_isint(left) && !xr_isfloat(left)) {
        return false;
    }
    if (!xr_isint(right) && !xr_isfloat(right)) {
        return false;
    }
    
    /* 2. 转换为浮点数进行计算 */
    double l = xr_isint(left) ? (double)xr_toint(left) : xr_tofloat(left);
    double r = xr_isint(right) ? (double)xr_toint(right) : xr_tofloat(right);
    double res;
    
    /* 3. 执行运算 */
    switch (op) {
        case TK_PLUS:
            res = l + r;
            break;
            
        case TK_MINUS:
            res = l - r;
            break;
            
        case TK_STAR:
            res = l * r;
            break;
            
        case TK_SLASH:
            /* 避免除零 */
            if (r == 0.0) {
                return false;
            }
            res = l / r;
            break;
            
        case TK_PERCENT:
            /* 避免除零 */
            if (r == 0.0) {
                return false;
            }
            res = fmod(l, r);
            break;
            
        default:
            /* 不支持的运算符（比较运算符在其他地方处理） */
            return false;
    }
    
    /* 4. 安全检查（参考Lua实现）
     *    避免NaN和-0.0问题
     *    NaN: 不确定的结果，运行时处理
     *    -0.0: 可能导致符号问题
     */
    if (isnan(res)) {
        return false;  /* 不折叠NaN */
    }
    
    /* 检查-0.0 */
    if (res == 0.0 && signbit(res)) {
        return false;  /* 不折叠-0.0 */
    }
    
    /* 5. 折叠成功 - 根据类型创建值 */
    /* 如果两个操作数都是整数且结果也是整数，保持整数类型 */
    if (xr_isint(left) && xr_isint(right) && res == (double)(long long)res) {
        *result = xr_int((xr_Integer)res);
    } else {
        *result = xr_float(res);
    }
    
    /* 统计 */
    g_opt_stats.fold_count++;
    g_opt_stats.fold_binary++;
    
    return true;
}

/*
** 折叠一元运算
*/
bool xr_opt_fold_unary(
    TokenType op,
    XrValue value,
    XrValue *result
) {
    switch (op) {
        case TK_MINUS:
            /* 数字取负 */
            if (xr_isint(value)) {
                xr_Integer num = xr_toint(value);
                if (num == 0) {
                    return false;  /* 避免-0 */
                }
                *result = xr_int(-num);
                g_opt_stats.fold_count++;
                g_opt_stats.fold_unary++;
                return true;
            }
            if (xr_isfloat(value)) {
                double num = xr_tofloat(value);
                if (num == 0.0) {
                    return false;  /* 避免-0.0 */
                }
                *result = xr_float(-num);
                g_opt_stats.fold_count++;
                g_opt_stats.fold_unary++;
                return true;
            }
            return false;
            
        case TK_NOT:
            /* 逻辑非 */
            if (xr_isbool(value)) {
                *result = xr_bool(!xr_tobool(value));
                g_opt_stats.fold_count++;
                g_opt_stats.fold_unary++;
                return true;
            }
            /* null也可以取反 */
            if (xr_isnull(value)) {
                *result = xr_bool(1);  /* !null = true */
                g_opt_stats.fold_count++;
                g_opt_stats.fold_unary++;
                return true;
            }
            return false;
            
        default:
            return false;
    }
}

/* ========== 优化统计 ========== */

void xr_opt_reset_stats(void) {
    g_opt_stats.fold_count = 0;
    g_opt_stats.fold_binary = 0;
    g_opt_stats.fold_unary = 0;
}

void xr_opt_print_stats(void) {
    if (g_opt_stats.fold_count > 0) {
        printf("\n=== 编译器优化统计 ===\n");
        printf("常量折叠总数: %d\n", g_opt_stats.fold_count);
        printf("  二元运算: %d\n", g_opt_stats.fold_binary);
        printf("  一元运算: %d\n", g_opt_stats.fold_unary);
        printf("====================\n");
    }
}


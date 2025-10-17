/*
** xinline.h
** Xray 函数内联分析器
** v0.15.0 - 函数内联候选识别
*/

#ifndef XINLINE_H
#define XINLINE_H

#include "xchunk.h"
#include <stdbool.h>

/* ========== 内联策略 ========== */

/* 内联判断阈值 */
#define INLINE_MAX_INSTRUCTIONS  10   /* 最大指令数 */
#define INLINE_MAX_PARAMS        4    /* 最大参数数 */
#define INLINE_MAX_LOCALS        8    /* 最大局部变量数 */

/* ========== 内联候选分析 ========== */

/*
** 内联候选信息
*/
typedef struct {
    bool can_inline;          /* 是否可以内联 */
    int instruction_count;    /* 指令数 */
    int param_count;          /* 参数数 */
    int local_count;          /* 局部变量数 */
    bool has_loops;           /* 是否有循环 */
    bool has_recursion;       /* 是否递归调用 */
    bool has_closure;         /* 是否有闭包 */
    int call_count;           /* 被调用次数（估计） */
} InlineCandidate;

/*
** 分析函数是否适合内联
** 
** 判断条件：
**   1. 指令数 <= INLINE_MAX_INSTRUCTIONS
**   2. 无循环（无JMP回跳）
**   3. 无递归调用
**   4. 无闭包创建
**   5. 参数数量合理
** 
** 参数：
**   proto - 要分析的函数原型
**   candidate - 输出的候选信息
** 
** 返回：
**   true如果适合内联
*/
bool xr_inline_analyze(Proto *proto, InlineCandidate *candidate);

/*
** 标记所有内联候选
** 
** 遍历Proto及其嵌套函数，标记所有内联候选
*/
int xr_inline_mark_candidates(Proto *proto);

/* ========== 辅助函数 ========== */

/*
** 检测函数是否有循环
** 检查是否有向后跳转的JMP指令
*/
bool xr_inline_has_loops(Proto *proto);

/*
** 检测函数是否有递归调用
** （简化版本：检查是否调用同名函数）
*/
bool xr_inline_has_recursion(Proto *proto);

/*
** 检测函数是否创建闭包
*/
bool xr_inline_has_closure(Proto *proto);

/*
** 计算函数的"复杂度"
** 返回值越小越适合内联
*/
int xr_inline_complexity(Proto *proto);

/* ========== 优化统计 ========== */

typedef struct {
    int total_functions;      /* 总函数数 */
    int inline_candidates;    /* 内联候选数 */
    int too_large;           /* 太大的函数数 */
    int has_loops;           /* 有循环的函数数 */
    int has_recursion;       /* 递归函数数 */
} InlineStats;

extern InlineStats g_inline_stats;

void xr_inline_reset_stats(void);
void xr_inline_print_stats(void);

#endif /* XINLINE_H */


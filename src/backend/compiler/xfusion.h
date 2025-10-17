/*
** xfusion.h
** Xray 指令融合优化器
** v0.15.0 - 指令模式识别与优化
*/

#ifndef XFUSION_H
#define XFUSION_H

#include "xchunk.h"
#include <stdbool.h>

/* ========== 指令融合优化器 ========== */

/*
** 对Proto进行指令融合优化
** 
** 优化包括：
**   1. LOADK常量优化：LOADK K(0/1/-1) => LOADI 0/1/-1
**   2. TEST+JMP融合：TEST + JMP => 优化的条件跳转
**   3. 算术立即数优化：ADD R, K(n) => ADDI R, n (如果n是小整数)
**   4. LOADK+运算融合：识别LOADK后跟运算的模式
** 
** 参数：
**   proto - 要优化的函数原型
** 
** 返回：
**   优化的次数
*/
int xr_fusion_optimize(Proto *proto);

/* ========== 子优化函数 ========== */

/*
** LOADK常量优化
** 
** 将加载常用常量的LOADK指令替换为更高效的LOADI指令
** 
** 示例：
**   LOADK R1, K(0)   => LOADI R1, 0
**   LOADK R1, K(1)   => LOADI R1, 1
**   LOADK R1, K(-1)  => LOADI R1, -1
*/
int xr_fusion_loadk_const(Proto *proto);

/*
** 算术立即数优化
** 
** 将LOADK + 算术运算优化为立即数运算
** 
** 示例：
**   LOADK R2, K(5)
**   ADD R1, R1, R2   => ADDI R1, R1, 5 (如果5在范围内)
*/
int xr_fusion_arith_imm(Proto *proto);

/*
** TEST+JMP融合
** 
** 优化连续的TEST和JMP指令
** 
** 示例：
**   TEST R1, k
**   JMP +offset      => 可以优化的条件跳转模式
*/
int xr_fusion_test_jmp(Proto *proto);

/*
** 比较常量优化
** 
** 将比较运算中的LOADK优化为立即数比较
** 
** 示例：
**   LOADK R2, K(0)
**   LT R1, R2        => LTI R1, 0
*/
int xr_fusion_cmp_const(Proto *proto);

/* ========== 辅助函数 ========== */

/*
** 检查常量值是否适合立即数
** 返回true且设置*imm如果可以用立即数表示
*/
bool xr_fusion_is_small_int(XrValue value, int *imm);

/*
** 检查两条指令是否可以融合
*/
bool xr_fusion_can_fuse(Instruction inst1, Instruction inst2);

/* ========== 优化统计 ========== */

typedef struct {
    int loadk_to_loadi;      /* LOADK转LOADI优化数 */
    int arith_to_imm;        /* 算术转立即数优化数 */
    int test_jmp_fused;      /* TEST+JMP融合数 */
    int cmp_to_imm;          /* 比较转立即数优化数 */
    int total_fusions;       /* 总融合次数 */
} FusionStats;

extern FusionStats g_fusion_stats;

void xr_fusion_reset_stats(void);
void xr_fusion_print_stats(void);

#endif /* XFUSION_H */


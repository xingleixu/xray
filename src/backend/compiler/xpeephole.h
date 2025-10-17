/*
** xpeephole.h
** Xray Peephole优化器
** v0.15.0 - 指令级优化
*/

#ifndef XPEEPHOLE_H
#define XPEEPHOLE_H

#include "xchunk.h"
#include <stdbool.h>

/* ========== Peephole优化器 ========== */

/*
** 对Proto进行Peephole优化
** 
** 优化包括：
**   1. 跳转链消除：JMP -> JMP -> target => JMP -> target
**   2. 冗余指令删除：重复的LOADK等
**   3. 死代码消除：JMP后的不可达代码
**   4. 无效MOVE消除：MOVE R[A], R[A]
** 
** 参数：
**   proto - 要优化的函数原型
** 
** 返回：
**   优化的次数
*/
int xr_peephole_optimize(Proto *proto);

/* ========== 子优化函数 ========== */

/*
** 跳转链消除
** 参考 Lua lcode.c:1904 fixjump()
** 
** 示例：
**   JMP +5
**   ...
**   JMP +10    ; pc=5
**   ...
**   target     ; pc=15
** 
** 优化后：
**   JMP +15    ; 直接跳到最终目标
*/
int xr_peep_jump_chain(Proto *proto);

/*
** 冗余指令删除
** 
** 示例：
**   LOADK R1, K(10)
**   LOADK R1, K(20)  ; 前一条指令无效
** 
** 优化后：
**   LOADK R1, K(20)
*/
int xr_peep_redundant(Proto *proto);

/*
** 死代码消除
** 
** 示例：
**   JMP +10
**   LOADK R1, K(10)  ; 不可达
**   ADD R2, R1, R3   ; 不可达
**   target:          ; pc=10
** 
** 优化后：
**   JMP +10
**   target:          ; pc=10
*/
int xr_peep_dead_code(Proto *proto);

/*
** 无效MOVE消除
** 
** 示例：
**   MOVE R1, R1      ; 无效
** 
** 优化后：
**   NOP  (或删除)
*/
int xr_peep_useless_move(Proto *proto);

/*
** NOP压缩
** 删除所有NOP指令并重新计算跳转偏移
** 这个优化应该在所有其他优化之后执行
** 
** 示例：
**   LOADK R1, K(10)
**   NOP              ; 被其他优化删除的指令
**   ADD R2, R1, R3
** 
** 优化后：
**   LOADK R1, K(10)
**   ADD R2, R1, R3
*/
int xr_peep_compress_nop(Proto *proto);

/* ========== 辅助函数 ========== */

/*
** 查找跳转的最终目标
** 跟随跳转链直到非JMP指令
*/
int xr_peep_finaltarget(Instruction *code, int pc, int size);

/*
** 检查指令是否是跳转指令
*/
bool xr_peep_is_jump(OpCode op);

/*
** 检查指令是否无副作用
*/
bool xr_peep_no_side_effect(OpCode op);

/* ========== 优化统计 ========== */

typedef struct {
    int jump_chain_opt;     /* 跳转链优化次数 */
    int redundant_removed;  /* 冗余指令删除数 */
    int dead_code_removed;  /* 死代码删除数 */
    int useless_move_removed; /* 无效MOVE删除数 */
    int nop_compressed;     /* NOP压缩数 */
    int total_optimizations; /* 总优化次数 */
} PeepholeStats;

extern PeepholeStats g_peephole_stats;

void xr_peephole_reset_stats(void);
void xr_peephole_print_stats(void);

#endif /* XPEEPHOLE_H */


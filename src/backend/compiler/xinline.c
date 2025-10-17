/*
** xinline.c
** Xray 函数内联分析器实现
** v0.15.0 - 函数内联候选识别
*/

#include "xinline.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 优化统计 */
InlineStats g_inline_stats = {0};

/* ========== 辅助函数 ========== */

/*
** 检测函数是否有循环（向后跳转）
*/
bool xr_inline_has_loops(Proto *proto) {
    for (int pc = 0; pc < proto->sizecode; pc++) {
        Instruction inst = proto->code[pc];
        OpCode op = GET_OPCODE(inst);
        
        if (op == OP_JMP) {
            int offset = GETARG_sJ(inst);
            int target = pc + 1 + offset;
            
            /* 向后跳转表示循环 */
            if (target <= pc) {
                return true;
            }
        }
    }
    return false;
}

/*
** 检测函数是否有递归调用
** （简化版本：检查是否有CALL指令调用自身）
*/
bool xr_inline_has_recursion(Proto *proto) {
    /* 简化实现：假设没有递归 */
    /* 真正的递归检测需要函数名和调用图分析 */
    return false;
}

/*
** 检测函数是否创建闭包
*/
bool xr_inline_has_closure(Proto *proto) {
    for (int pc = 0; pc < proto->sizecode; pc++) {
        Instruction inst = proto->code[pc];
        OpCode op = GET_OPCODE(inst);
        
        if (op == OP_CLOSURE) {
            return true;
        }
    }
    return false;
}

/*
** 计算函数复杂度
** 返回值：0-100，越小越适合内联
*/
int xr_inline_complexity(Proto *proto) {
    int complexity = 0;
    
    /* 基础复杂度 = 指令数 */
    complexity += proto->sizecode;
    
    /* 参数和局部变量增加复杂度 */
    complexity += proto->numparams * 2;
    
    /* 嵌套函数增加复杂度 */
    complexity += proto->sizeprotos * 5;
    
    /* Upvalue增加复杂度 */
    complexity += proto->sizeupvalues * 3;
    
    /* 分支和跳转增加复杂度 */
    for (int pc = 0; pc < proto->sizecode; pc++) {
        OpCode op = GET_OPCODE(proto->code[pc]);
        if (op == OP_JMP || op == OP_TEST || op == OP_TESTSET) {
            complexity += 2;
        }
    }
    
    return complexity;
}

/* ========== 内联分析 ========== */

/*
** 分析函数是否适合内联
*/
bool xr_inline_analyze(Proto *proto, InlineCandidate *candidate) {
    if (!proto || !candidate) {
        return false;
    }
    
    /* 初始化候选信息 */
    memset(candidate, 0, sizeof(InlineCandidate));
    
    /* 统计基本信息 */
    candidate->instruction_count = proto->sizecode;
    candidate->param_count = proto->numparams;
    candidate->local_count = proto->maxstacksize;
    
    /* 检测特征 */
    candidate->has_loops = xr_inline_has_loops(proto);
    candidate->has_recursion = xr_inline_has_recursion(proto);
    candidate->has_closure = xr_inline_has_closure(proto);
    
    /* 判断是否可以内联 */
    candidate->can_inline = true;
    
    /* 规则1: 指令数不能太多 */
    if (candidate->instruction_count > INLINE_MAX_INSTRUCTIONS) {
        candidate->can_inline = false;
        g_inline_stats.too_large++;
    }
    
    /* 规则2: 不能有循环 */
    if (candidate->has_loops) {
        candidate->can_inline = false;
        g_inline_stats.has_loops++;
    }
    
    /* 规则3: 不能递归 */
    if (candidate->has_recursion) {
        candidate->can_inline = false;
        g_inline_stats.has_recursion++;
    }
    
    /* 规则4: 参数不能太多 */
    if (candidate->param_count > INLINE_MAX_PARAMS) {
        candidate->can_inline = false;
    }
    
    /* 规则5: 不应该有闭包（简化） */
    if (candidate->has_closure) {
        candidate->can_inline = false;
    }
    
    /* 更新统计 */
    g_inline_stats.total_functions++;
    if (candidate->can_inline) {
        g_inline_stats.inline_candidates++;
    }
    
    return candidate->can_inline;
}

/*
** 标记所有内联候选
*/
int xr_inline_mark_candidates(Proto *proto) {
    if (!proto) {
        return 0;
    }
    
    int candidate_count = 0;
    InlineCandidate candidate;
    
    /* 分析当前函数 */
    if (xr_inline_analyze(proto, &candidate)) {
        candidate_count++;
        
        /* 可以在这里标记Proto（需要扩展Proto结构）*/
        /* 目前只做统计 */
    }
    
    /* 递归分析嵌套函数 */
    for (int i = 0; i < proto->sizeprotos; i++) {
        candidate_count += xr_inline_mark_candidates(proto->protos[i]);
    }
    
    return candidate_count;
}

/* ========== 统计 ========== */

void xr_inline_reset_stats(void) {
    memset(&g_inline_stats, 0, sizeof(InlineStats));
}

void xr_inline_print_stats(void) {
    if (g_inline_stats.total_functions > 0) {
        printf("\n=== 函数内联分析 ===\n");
        printf("总函数数: %d\n", g_inline_stats.total_functions);
        printf("内联候选: %d\n", g_inline_stats.inline_candidates);
        printf("太大的函数: %d\n", g_inline_stats.too_large);
        printf("有循环的函数: %d\n", g_inline_stats.has_loops);
        printf("递归函数: %d\n", g_inline_stats.has_recursion);
        
        if (g_inline_stats.inline_candidates > 0) {
            int percentage = (g_inline_stats.inline_candidates * 100) / 
                            g_inline_stats.total_functions;
            printf("内联比例: %d%%\n", percentage);
        }
        printf("==================\n");
    }
}


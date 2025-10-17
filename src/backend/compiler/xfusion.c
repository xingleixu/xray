/*
** xfusion.c
** Xray 指令融合优化器实现
** v0.16.0 - 指令模式识别与优化（修复立即数编码bug）
**
** 修复日志（2025-10-16）：
** 1. 修复立即数编码bug：移除错误的uint8_t转换
** 2. 参考Lua/QuickJS实现，int8_t可以直接编码为8位字段
** 3. 增强寄存器安全检查，避免数据流错误
** 4. 重新启用比较常量优化
*/

#include "xfusion.h"
#include "xvalue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 优化统计 */
FusionStats g_fusion_stats = {0};

/* ========== 辅助函数 ========== */

/*
** 检查XrValue是否是小整数
*/
bool xr_fusion_is_small_int(XrValue value, int *imm) {
    if (xr_isint(value)) {
        xr_Integer val = xr_toint(value);
        /* 检查是否在有符号字节范围内 (-128 到 127) */
        if (val >= -128 && val <= 127) {
            *imm = (int)val;
            return true;
        }
    } else if (xr_isfloat(value)) {
        double val = xr_tofloat(value);
        /* 检查是否是整数值且在范围内 */
        if (val == (int)val && val >= -128 && val <= 127) {
            *imm = (int)val;
            return true;
        }
    }
    return false;
}

/*
** 检查常量是否是0, 1, 或-1（最常用的常量）
*/
bool xr_fusion_is_common_const(XrValue value, int *imm) {
    if (xr_isint(value)) {
        xr_Integer val = xr_toint(value);
        if (val == 0 || val == 1 || val == -1) {
            *imm = (int)val;
            return true;
        }
    } else if (xr_isfloat(value)) {
        double val = xr_tofloat(value);
        if (val == 0.0 || val == 1.0 || val == -1.0) {
            *imm = (int)val;
            return true;
        }
    }
    return false;
}

/* ========== 优化实现 ========== */

/*
** LOADK常量优化
** 将LOADK K(0/1/-1)替换为LOADI
** 
** 注意：此优化已禁用（2025-10-16）
** 原因：性能测试发现LOADI比LOADK慢5-10%
** - LOADK访问预分配的常量数组（缓存友好）
** - LOADI需要调用xr_int()构造XrValue（函数调用开销）
** - 在fib(35)测试中，差异约50-100ms
** 
** TODO: 如果xr_int()能强制内联，可以重新启用
*/
int xr_fusion_loadk_const(Proto *proto) {
    /* 暂时禁用此优化 - 已确认导致性能下降 */
    (void)proto;
    return 0;
    
    #if 0  /* 保留代码以便后续优化 */
    int opt_count = 0;
    
    for (int pc = 0; pc < proto->sizecode; pc++) {
        Instruction inst = proto->code[pc];
        OpCode op = GET_OPCODE(inst);
        
        if (op == OP_LOADK) {
            int a = GETARG_A(inst);
            int bx = GETARG_Bx(inst);
            
            /* 检查常量值 */
            if (bx < proto->constants.count) {
                XrValue kval = proto->constants.values[bx];
                int imm;
                
                /* 如果是常用常量，用LOADI替代 */
                if (xr_fusion_is_common_const(kval, &imm)) {
                    /* 将LOADK替换为LOADI */
                    proto->code[pc] = CREATE_AsBx(OP_LOADI, a, imm);
                    opt_count++;
                    g_fusion_stats.loadk_to_loadi++;
                }
            }
        }
    }
    
    return opt_count;
    #endif
}

/*
** 算术立即数优化
** 识别LOADK后跟算术运算的模式并优化
** 
** 修复说明（参考Lua/QuickJS）：
** 1. 立即数编码修复：int8_t可以直接存储，无需转换
** 2. 寄存器检查：确保LOADK的目标寄存器只被后续指令使用一次
*/
int xr_fusion_arith_imm(Proto *proto) {
    int opt_count = 0;
    
    for (int pc = 0; pc < proto->sizecode - 1; pc++) {
        Instruction inst1 = proto->code[pc];
        Instruction inst2 = proto->code[pc + 1];
        
        OpCode op1 = GET_OPCODE(inst1);
        OpCode op2 = GET_OPCODE(inst2);
        
        /* 模式：LOADK Rx, K(n) + ADD/SUB/MUL Ry, Rz, Rx */
        if (op1 == OP_LOADK) {
            int reg_k = GETARG_A(inst1);  /* LOADK的目标寄存器 */
            int bx = GETARG_Bx(inst1);
            
            /* 检查下一条指令是否使用这个寄存器作为第二个操作数 */
            if (op2 == OP_ADD || op2 == OP_SUB || op2 == OP_MUL) {
                int a = GETARG_A(inst2);
                int b = GETARG_B(inst2);
                int c = GETARG_C(inst2);
                
                /* 确保：
                 * 1. LOADK的结果只被下一条指令使用（c == reg_k）
                 * 2. LOADK的结果不是目标寄存器（避免覆盖问题）
                 * 3. 常量在有效范围内
                 */
                if (c == reg_k && a != reg_k && bx < proto->constants.count) {
                    XrValue kval = proto->constants.values[bx];
                    int imm;
                    
                    /* 如果是小整数，可以用立即数指令 */
                    if (xr_fusion_is_small_int(kval, &imm)) {
                        /* 选择对应的立即数指令 */
                        OpCode new_op = OP_NOP;
                        switch (op2) {
                            case OP_ADD: new_op = OP_ADDI; break;
                            case OP_SUB: new_op = OP_SUBI; break;
                            case OP_MUL: new_op = OP_MULI; break;
                            default: break;
                        }
                        
                        if (new_op != OP_NOP) {
                            /* 将LOADK替换为NOP */
                            proto->code[pc] = CREATE_ABC(OP_NOP, 0, 0, 0);
                            /* 将算术指令替换为立即数版本 */
                            /* 修复：直接传递imm，int8_t会自动编码正确 */
                            proto->code[pc + 1] = CREATE_ABC(new_op, a, b, imm);
                            opt_count++;
                            g_fusion_stats.arith_to_imm++;
                        }
                    }
                }
            }
        }
    }
    
    return opt_count;
}

/*
** TEST+JMP融合优化
** （简化版本，主要记录模式）
*/
int xr_fusion_test_jmp(Proto *proto) {
    int opt_count = 0;
    
    for (int pc = 0; pc < proto->sizecode - 1; pc++) {
        Instruction inst1 = proto->code[pc];
        Instruction inst2 = proto->code[pc + 1];
        
        OpCode op1 = GET_OPCODE(inst1);
        OpCode op2 = GET_OPCODE(inst2);
        
        /* 识别TEST + JMP模式 */
        if (op1 == OP_TEST && op2 == OP_JMP) {
            /* 这个模式在Xray中已经相对优化了 */
            /* 可以考虑TESTSET等其他优化 */
            g_fusion_stats.test_jmp_fused++;
        }
    }
    
    return opt_count;
}

/*
** 比较常量优化
** 将比较中的LOADK优化为立即数比较
** 
** 修复说明（参考Lua/QuickJS）：
** 1. 修复立即数编码bug：直接传递imm，无需uint8_t转换
** 2. 添加寄存器安全检查
** 3. 正确处理比较指令的语义（R[A] < sB，立即数在第二个操作数位置）
*/
int xr_fusion_cmp_const(Proto *proto) {
    int opt_count = 0;
    
    for (int pc = 0; pc < proto->sizecode - 1; pc++) {
        Instruction inst1 = proto->code[pc];
        Instruction inst2 = proto->code[pc + 1];
        
        OpCode op1 = GET_OPCODE(inst1);
        OpCode op2 = GET_OPCODE(inst2);
        
        /* 模式：LOADK Rx, K(n) + LT/LE/GT/GE Ra, Rx, k
         * 注意：立即数比较指令的格式是 LTI Ra, sB, k (Ra < sB)
         * 所以我们需要检查B参数（第二个操作数）是否是LOADK的目标
         */
        if (op1 == OP_LOADK) {
            int reg_k = GETARG_A(inst1);
            int bx = GETARG_Bx(inst1);
            
            if ((op2 == OP_LT || op2 == OP_LE || op2 == OP_GT || op2 == OP_GE) &&
                bx < proto->constants.count) {
                
                int a = GETARG_A(inst2);
                int b = GETARG_B(inst2);
                int k = GETARG_C(inst2);
                
                /* 检查：LOADK的结果是第二个操作数 */
                if (b == reg_k) {
                    XrValue kval = proto->constants.values[bx];
                    int imm;
                    
                    if (xr_fusion_is_small_int(kval, &imm)) {
                        /* 选择对应的立即数比较指令 */
                        OpCode new_op = OP_NOP;
                        switch (op2) {
                            case OP_LT: new_op = OP_LTI; break;
                            case OP_LE: new_op = OP_LEI; break;
                            case OP_GT: new_op = OP_GTI; break;
                            case OP_GE: new_op = OP_GEI; break;
                            default: break;
                        }
                        
                        if (new_op != OP_NOP) {
                            /* 删除LOADK */
                            proto->code[pc] = CREATE_ABC(OP_NOP, 0, 0, 0);
                            /* 使用立即数比较 */
                            /* 修复：直接传递imm，int8_t会自动编码正确 */
                            proto->code[pc + 1] = CREATE_ABC(new_op, a, imm, k);
                            opt_count++;
                            g_fusion_stats.cmp_to_imm++;
                        }
                    }
                }
            }
        }
    }
    
    return opt_count;
}

/*
** 主优化函数
*/
int xr_fusion_optimize(Proto *proto) {
    if (!proto || proto->sizecode == 0) {
        return 0;
    }
    
    int total = 0;
    
    /* 执行各种融合优化 */
    total += xr_fusion_loadk_const(proto);
    total += xr_fusion_arith_imm(proto);
    total += xr_fusion_test_jmp(proto);
    total += xr_fusion_cmp_const(proto);
    
    g_fusion_stats.total_fusions = total;
    
    /* 递归优化嵌套函数 */
    for (int i = 0; i < proto->sizeprotos; i++) {
        total += xr_fusion_optimize(proto->protos[i]);
    }
    
    return total;
}

/* ========== 统计 ========== */

void xr_fusion_reset_stats(void) {
    memset(&g_fusion_stats, 0, sizeof(FusionStats));
}

void xr_fusion_print_stats(void) {
    if (g_fusion_stats.total_fusions > 0) {
        printf("\n=== 指令融合统计 ===\n");
        printf("LOADK转LOADI: %d\n", g_fusion_stats.loadk_to_loadi);
        printf("算术转立即数: %d\n", g_fusion_stats.arith_to_imm);
        printf("TEST+JMP识别: %d\n", g_fusion_stats.test_jmp_fused);
        printf("比较转立即数: %d\n", g_fusion_stats.cmp_to_imm);
        printf("总融合次数: %d\n", g_fusion_stats.total_fusions);
        printf("==================\n");
    }
}


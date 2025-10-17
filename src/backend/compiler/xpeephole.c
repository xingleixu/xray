/*
** xpeephole.c
** Xray Peephole优化器实现
** v0.15.0 - 指令级优化
*/

#include "xpeephole.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 优化统计 */
PeepholeStats g_peephole_stats = {0};

/* ========== 辅助函数 ========== */

/*
** 检查是否是跳转指令
*/
bool xr_peep_is_jump(OpCode op) {
    return op == OP_JMP;
}

/*
** 检查指令是否无副作用（可以安全删除）
*/
bool xr_peep_no_side_effect(OpCode op) {
    switch (op) {
        case OP_MOVE:
        case OP_LOADI:
        case OP_LOADF:
        case OP_LOADK:
        case OP_LOADNIL:
        case OP_LOADTRUE:
        case OP_LOADFALSE:
        case OP_ADD:
        case OP_ADDI:
        case OP_ADDK:
        case OP_SUB:
        case OP_SUBI:
        case OP_SUBK:
        case OP_MUL:
        case OP_MULI:
        case OP_MULK:
        case OP_DIV:
        case OP_DIVK:
        case OP_MOD:
        case OP_MODK:
        case OP_UNM:
        case OP_NOT:
        case OP_NOP:
            return true;
        default:
            return false;
    }
}

/*
** 查找跳转的最终目标（跟随跳转链）
** 参考 Lua lcode.c:1864 finaltarget()
*/
int xr_peep_finaltarget(Instruction *code, int pc, int size) {
    int target = pc;
    int count = 0;
    
    /* 限制查找深度，防止无限循环 */
    while (count < 100 && target >= 0 && target < size) {
        Instruction inst = code[target];
        OpCode op = GET_OPCODE(inst);
        
        if (op != OP_JMP) {
            break;  /* 找到最终目标 */
        }
        
        /* 跟随跳转 */
        int offset = GETARG_sJ(inst);
        target = target + 1 + offset;
        count++;
    }
    
    return target;
}

/* ========== 优化实现 ========== */

/*
** 跳转链消除
** 参考 Lua lcode.c:1904 fixjump()
*/
int xr_peep_jump_chain(Proto *proto) {
    int opt_count = 0;
    
    for (int pc = 0; pc < proto->sizecode; pc++) {
        Instruction inst = proto->code[pc];
        OpCode op = GET_OPCODE(inst);
        
        if (op == OP_JMP) {
            /* 计算跳转目标 */
            int offset = GETARG_sJ(inst);
            int target = pc + 1 + offset;
            
            /* 查找最终目标 */
            int final = xr_peep_finaltarget(proto->code, target, proto->sizecode);
            
            /* 如果最终目标不同，更新跳转 */
            if (final != target && final >= 0 && final < proto->sizecode) {
                int new_offset = final - pc - 1;
                
                /* 检查偏移量是否在范围内 */
                if (new_offset >= -MAXARG_sJ && new_offset <= MAXARG_sJ) {
                    proto->code[pc] = CREATE_sJ(OP_JMP, new_offset);
                    opt_count++;
                    g_peephole_stats.jump_chain_opt++;
                }
            }
        }
    }
    
    return opt_count;
}

/*
** 冗余指令删除
** 
** 删除会被覆盖的指令
*/
int xr_peep_redundant(Proto *proto) {
    int opt_count = 0;
    
    for (int pc = 0; pc < proto->sizecode - 1; pc++) {
        Instruction inst1 = proto->code[pc];
        Instruction inst2 = proto->code[pc + 1];
        
        OpCode op1 = GET_OPCODE(inst1);
        OpCode op2 = GET_OPCODE(inst2);
        
        /* 检测模式：写入同一寄存器的连续指令 */
        if (xr_peep_no_side_effect(op1) && xr_peep_no_side_effect(op2)) {
            int a1 = GETARG_A(inst1);
            int a2 = GETARG_A(inst2);
            
            /* 如果写入同一寄存器，第一条指令无效 */
            if (a1 == a2) {
                /* 将第一条指令替换为NOP */
                proto->code[pc] = CREATE_ABC(OP_NOP, 0, 0, 0);
                opt_count++;
                g_peephole_stats.redundant_removed++;
            }
        }
    }
    
    return opt_count;
}

/*
** 死代码消除
** 
** 删除JMP后的不可达代码
** 参考 Lua lcode.c 的可达性分析
*/
int xr_peep_dead_code(Proto *proto) {
    int opt_count = 0;
    
    if (!proto || proto->sizecode < 2) {
        return 0;
    }
    
    /* 创建可达性标记数组 */
    bool *reachable = (bool*)calloc(proto->sizecode, sizeof(bool));
    if (!reachable) {
        return 0;
    }
    
    /* 第一轮：标记所有可达指令 */
    /* 起点：第一条指令总是可达的 */
    reachable[0] = true;
    
    /* 遍历所有指令 */
    for (int pc = 0; pc < proto->sizecode; pc++) {
        if (!reachable[pc]) {
            continue;  /* 如果当前指令不可达，跳过 */
        }
        
        Instruction inst = proto->code[pc];
        OpCode op = GET_OPCODE(inst);
        
        /* 根据指令类型标记后续可达指令 */
        switch (op) {
            case OP_JMP: {
                /* 跳转指令：标记目标为可达 */
                int offset = GETARG_sJ(inst);
                int target = pc + 1 + offset;
                if (target >= 0 && target < proto->sizecode) {
                    reachable[target] = true;
                }
                /* JMP不会继续执行下一条，所以不标记pc+1 */
                break;
            }
            
            case OP_RETURN:
            case OP_TAILCALL:
                /* 返回指令：不会执行下一条 */
                break;
            
            /* 条件跳转：既标记跳转目标，也标记下一条 */
            case OP_EQ:
            case OP_EQK:
            case OP_EQI:
            case OP_LT:
            case OP_LTI:
            case OP_LE:
            case OP_LEI:
            case OP_GT:
            case OP_GTI:
            case OP_GE:
            case OP_GEI:
            case OP_TEST:
            case OP_TESTSET:
                /* 条件跳转会跳过下一条指令，标记pc+2 */
                if (pc + 2 < proto->sizecode) {
                    reachable[pc + 2] = true;
                }
                /* 也标记不跳转的情况（pc+1） */
                if (pc + 1 < proto->sizecode) {
                    reachable[pc + 1] = true;
                }
                break;
            
            default:
                /* 普通指令：顺序执行到下一条 */
                if (pc + 1 < proto->sizecode) {
                    reachable[pc + 1] = true;
                }
                break;
        }
    }
    
    /* 第二轮：删除不可达的指令（用NOP替换） */
    for (int pc = 0; pc < proto->sizecode; pc++) {
        if (!reachable[pc]) {
            Instruction inst = proto->code[pc];
            OpCode op = GET_OPCODE(inst);
            
            /* 只删除无副作用的不可达指令 */
            if (xr_peep_no_side_effect(op) || op == OP_JMP) {
                proto->code[pc] = CREATE_ABC(OP_NOP, 0, 0, 0);
                opt_count++;
                g_peephole_stats.dead_code_removed++;
            }
        }
    }
    
    free(reachable);
    return opt_count;
}

/*
** 无效MOVE消除
*/
int xr_peep_useless_move(Proto *proto) {
    int opt_count = 0;
    
    for (int pc = 0; pc < proto->sizecode; pc++) {
        Instruction inst = proto->code[pc];
        OpCode op = GET_OPCODE(inst);
        
        if (op == OP_MOVE) {
            int a = GETARG_A(inst);
            int b = GETARG_B(inst);
            
            /* MOVE R[A], R[A] 是无效的 */
            if (a == b) {
                proto->code[pc] = CREATE_ABC(OP_NOP, 0, 0, 0);
                opt_count++;
                g_peephole_stats.useless_move_removed++;
            }
        }
    }
    
    return opt_count;
}

/*
** NOP压缩
** 删除所有NOP指令并重新计算跳转偏移
*/
int xr_peep_compress_nop(Proto *proto) {
    if (!proto || proto->sizecode == 0) {
        return 0;
    }
    
    /* 统计NOP数量 */
    int nop_count = 0;
    for (int i = 0; i < proto->sizecode; i++) {
        if (GET_OPCODE(proto->code[i]) == OP_NOP) {
            nop_count++;
        }
    }
    
    /* 如果没有NOP，直接返回 */
    if (nop_count == 0) {
        return 0;
    }
    
    /* 创建PC映射表：old_pc -> new_pc */
    int *pc_map = (int*)malloc(proto->sizecode * sizeof(int));
    if (!pc_map) {
        return 0;
    }
    
    /* 创建新的指令和行号数组 */
    int new_size = proto->sizecode - nop_count;
    Instruction *new_code = (Instruction*)malloc(new_size * sizeof(Instruction));
    int *new_lineinfo = proto->lineinfo ? (int*)malloc(new_size * sizeof(int)) : NULL;
    
    if (!new_code || (proto->lineinfo && !new_lineinfo)) {
        free(pc_map);
        free(new_code);
        free(new_lineinfo);
        return 0;
    }
    
    /* 复制非NOP指令并建立映射 */
    int new_pc = 0;
    for (int old_pc = 0; old_pc < proto->sizecode; old_pc++) {
        pc_map[old_pc] = new_pc;
        
        Instruction inst = proto->code[old_pc];
        OpCode op = GET_OPCODE(inst);
        
        if (op != OP_NOP) {
            new_code[new_pc] = inst;
            if (new_lineinfo && proto->lineinfo) {
                new_lineinfo[new_pc] = proto->lineinfo[old_pc];
            }
            new_pc++;
        }
    }
    
    /* 更新所有跳转指令的偏移 */
    for (int pc = 0; pc < new_size; pc++) {
        Instruction inst = new_code[pc];
        OpCode op = GET_OPCODE(inst);
        
        /* 处理JMP指令 */
        if (op == OP_JMP) {
            int old_offset = GETARG_sJ(inst);
            
            /* 找到原始PC（反向映射） */
            int orig_pc = -1;
            for (int i = 0; i < proto->sizecode; i++) {
                if (pc_map[i] == pc) {
                    orig_pc = i;
                    break;
                }
            }
            
            if (orig_pc >= 0) {
                int old_target = orig_pc + 1 + old_offset;
                
                /* 确保目标在范围内 */
                if (old_target >= 0 && old_target < proto->sizecode) {
                    int new_target = pc_map[old_target];
                    int new_offset = new_target - pc - 1;
                    
                    /* 检查偏移是否在范围内 */
                    if (new_offset >= -MAXARG_sJ && new_offset <= MAXARG_sJ) {
                        new_code[pc] = CREATE_sJ(OP_JMP, new_offset);
                    }
                }
            }
        }
    }
    
    /* 替换Proto中的代码 */
    free(proto->code);
    proto->code = new_code;
    proto->sizecode = new_size;
    proto->capacity_code = new_size;
    
    /* 替换行号信息 */
    if (proto->lineinfo) {
        free(proto->lineinfo);
        proto->lineinfo = new_lineinfo;
        proto->size_lineinfo = new_size;
        proto->capacity_lineinfo = new_size;
    }
    
    free(pc_map);
    
    g_peephole_stats.nop_compressed += nop_count;
    return nop_count;
}

/*
** 主优化函数
*/
int xr_peephole_optimize(Proto *proto) {
    if (!proto || proto->sizecode == 0) {
        return 0;
    }
    
    int total = 0;
    
    /* 执行各种优化（生成NOP） */
    total += xr_peep_jump_chain(proto);
    total += xr_peep_redundant(proto);
    total += xr_peep_dead_code(proto);
    total += xr_peep_useless_move(proto);
    
    /* NOP压缩（删除所有NOP）- 重新启用 */
    total += xr_peep_compress_nop(proto);
    
    g_peephole_stats.total_optimizations = total;
    
    /* 递归优化嵌套函数 */
    for (int i = 0; i < proto->sizeprotos; i++) {
        total += xr_peephole_optimize(proto->protos[i]);
    }
    
    return total;
}

/* ========== 统计 ========== */

void xr_peephole_reset_stats(void) {
    memset(&g_peephole_stats, 0, sizeof(PeepholeStats));
}

void xr_peephole_print_stats(void) {
    if (g_peephole_stats.total_optimizations > 0) {
        printf("\n=== Peephole优化统计 ===\n");
        printf("跳转链优化: %d\n", g_peephole_stats.jump_chain_opt);
        printf("冗余指令删除: %d\n", g_peephole_stats.redundant_removed);
        printf("死代码删除: %d\n", g_peephole_stats.dead_code_removed);
        printf("无效MOVE删除: %d\n", g_peephole_stats.useless_move_removed);
        printf("NOP压缩: %d\n", g_peephole_stats.nop_compressed);
        printf("总优化次数: %d\n", g_peephole_stats.total_optimizations);
        printf("=======================\n");
    }
}


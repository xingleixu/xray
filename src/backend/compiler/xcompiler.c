/*
** xcompiler.c
** Xray 寄存器编译器实现
*/

#include "xcompiler.h"
#include "xcompiler_context.h"
#include "xoptimize.h"
#include "xpeephole.h"
#include "xfusion.h"
#include "xinline.h"
#include "xmem.h"
#include "xstring.h"
#include "xsymbol.h"  /* v0.20.0: Symbol系统支持 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* ========== 全局变量已删除 ========== */
/* 
 * v0.14.0 重构：所有状态移到CompilerContext
 * - current → ctx->current
 * - current_line → ctx->current_line  
 * - global_vars → ctx->global_vars
 * - global_var_count → ctx->global_var_count
 */

/* ========== 前向声明 ========== */

static int get_or_add_global(CompilerContext *ctx, Compiler *compiler, XrString *name);

static int compile_literal(CompilerContext *ctx, Compiler *compiler, LiteralNode *node);
static int compile_binary(CompilerContext *ctx, Compiler *compiler, BinaryNode *node, AstNodeType type);
static int compile_comparison(CompilerContext *ctx, Compiler *compiler, BinaryNode *node, AstNodeType type);
static int compile_unary(CompilerContext *ctx, Compiler *compiler, UnaryNode *node, AstNodeType type);
static int compile_variable(CompilerContext *ctx, Compiler *compiler, VariableNode *node);
static void compile_assignment(CompilerContext *ctx, Compiler *compiler, AssignmentNode *node);
static void compile_print(CompilerContext *ctx, Compiler *compiler, PrintNode *node);
static void compile_if(CompilerContext *ctx, Compiler *compiler, IfStmtNode *node);
static void compile_while(CompilerContext *ctx, Compiler *compiler, WhileStmtNode *node);
static void compile_for(CompilerContext *ctx, Compiler *compiler, ForStmtNode *node);
static int compile_call(CompilerContext *ctx, Compiler *compiler, CallExprNode *node);
static void compile_function(CompilerContext *ctx, Compiler *compiler, FunctionDeclNode *node);
static void compile_return(CompilerContext *ctx, Compiler *compiler, ReturnStmtNode *node);
static int compile_array_literal(CompilerContext *ctx, Compiler *compiler, ArrayLiteralNode *node);
static int compile_index_get(CompilerContext *ctx, Compiler *compiler, IndexGetNode *node);
static void compile_index_set(CompilerContext *ctx, Compiler *compiler, IndexSetNode *node);

/* OOP相关编译函数（v0.19.0新增）*/
static void compile_class(CompilerContext *ctx, Compiler *compiler, ClassDeclNode *node);
static int compile_new_expr(CompilerContext *ctx, Compiler *compiler, NewExprNode *node);
static int compile_member_access(CompilerContext *ctx, Compiler *compiler, MemberAccessNode *node);
static void compile_member_set(CompilerContext *ctx, Compiler *compiler, MemberSetNode *node);

/* ========== 错误处理 ========== */

/*
** 报告编译错误
*/
void xr_compiler_error(CompilerContext *ctx, Compiler *compiler, const char *format, ...) {
    if (compiler->panic_mode) {
        return;  /* 避免错误雪崩 */
    }
    compiler->panic_mode = true;
    compiler->had_error = true;
    
    fprintf(stderr, "[line %d] Error: ", ctx->current_line);
    
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    
    fprintf(stderr, "\n");
}

/* ========== 寄存器分配 ========== */

/*
** 分配临时寄存器
*/
int xr_allocreg(CompilerContext *ctx, Compiler *compiler) {
    RegState *rs = &compiler->rs;
    if (rs->freereg >= MAXREGS) {
        xr_compiler_error(ctx, compiler, "Too many registers (max %d)", MAXREGS);
        return 0;
    }
    return rs->freereg++;
}

/*
** 释放临时寄存器
*/
void xr_freereg(Compiler *compiler, int reg) {
    RegState *rs = &compiler->rs;
    /* 只释放临时寄存器（大于等于nactvar的寄存器） */
    if (reg >= rs->nactvar && reg == rs->freereg - 1) {
        rs->freereg = reg;
    }
}

/*
** 保留寄存器（用于局部变量）
*/
void xr_reservereg(Compiler *compiler) {
    RegState *rs = &compiler->rs;
    rs->nactvar = rs->freereg;
}

/* ========== 指令发射 ========== */

/*
** 发射指令到当前Proto
*/
static void emit_instruction(CompilerContext *ctx, Compiler *compiler, Instruction inst) {
    xr_bc_proto_write(compiler->proto, inst, ctx->current_line);
}

/*
** 发射ABC格式指令
*/
void xr_emit_ABC(CompilerContext *ctx, Compiler *compiler, OpCode op, int a, int b, int c) {
    Instruction inst = CREATE_ABC(op, a, b, c);
    emit_instruction(ctx, compiler, inst);
}

/*
** 发射ABsC格式指令（B是寄存器，C是有符号立即数）
*/
void xr_emit_ABsC(CompilerContext *ctx, Compiler *compiler, OpCode op, int a, int b, int sc) {
    /* 将有符号整数转换为无符号8位（模256） */
    uint8_t c = (uint8_t)(sc & 0xFF);
    Instruction inst = CREATE_ABC(op, a, b, c);
    emit_instruction(ctx, compiler, inst);
}

/*
** 发射ABx格式指令
*/
void xr_emit_ABx(CompilerContext *ctx, Compiler *compiler, OpCode op, int a, int bx) {
    Instruction inst = CREATE_ABx(op, a, bx);
    emit_instruction(ctx, compiler, inst);
}

/*
** 发射AsBx格式指令
*/
void xr_emit_AsBx(CompilerContext *ctx, Compiler *compiler, OpCode op, int a, int sbx) {
    Instruction inst = CREATE_AsBx(op, a, sbx);
    emit_instruction(ctx, compiler, inst);
}

/*
** 发射跳转指令（返回跳转位置）
*/
int xr_emit_jump(CompilerContext *ctx, Compiler *compiler, OpCode op) {
    /* 发射占位符跳转指令 */
    xr_emit_ABC(ctx, compiler, op, 0, 0, 0);  /* 跳转偏移稍后回填 */
    return compiler->proto->sizecode - 1;
}

/*
** 回填跳转偏移
*/
void xr_patch_jump(CompilerContext *ctx, Compiler *compiler, int offset) {
    /* 计算跳转距离 */
    int jump = compiler->proto->sizecode - offset - 1;
    
    if (jump > MAXARG_sJ) {
        xr_compiler_error(ctx, compiler, "Too much code to jump over");
    }
    
    /* 回填跳转指令 */
    Instruction *inst = &compiler->proto->code[offset];
    *inst = CREATE_sJ(GET_OPCODE(*inst), jump);
}

/*
** 发射循环指令（向后跳转）
*/
void xr_emit_loop(CompilerContext *ctx, Compiler *compiler, int loop_start) {
    int offset = compiler->proto->sizecode - loop_start + 1;
    if (offset > MAXARG_sJ) {
        xr_compiler_error(ctx, compiler, "Loop body too large");
    }
    
    xr_emit_ABC(ctx, compiler, OP_JMP, 0, 0, 0);
    
    /* 设置向后跳转 */
    int jump_inst = compiler->proto->sizecode - 1;
    compiler->proto->code[jump_inst] = CREATE_sJ(OP_JMP, -offset);
}

/* ========== 作用域管理 ========== */

/*
** 进入新作用域
*/
void xr_begin_scope(Compiler *compiler) {
    compiler->scope_depth++;
}

/*
** 离开作用域
*/
void xr_end_scope(CompilerContext *ctx, Compiler *compiler) {
    compiler->scope_depth--;
    
    /* 释放当前作用域的局部变量 */
    while (compiler->local_count > 0 &&
           compiler->locals[compiler->local_count - 1].depth > compiler->scope_depth) {
        
        Local *local = &compiler->locals[compiler->local_count - 1];
        
        /* 如果变量被捕获，关闭upvalue */
        if (local->is_captured) {
            xr_emit_ABC(ctx, compiler, OP_CLOSE, local->reg, 0, 0);
        }
        
        compiler->local_count--;
    }
    
    /* 更新活跃变量数 */
    compiler->rs.nactvar = (compiler->local_count > 0) 
        ? compiler->locals[compiler->local_count - 1].reg + 1 
        : 0;
    compiler->rs.freereg = compiler->rs.nactvar;
}

/*
** 定义局部变量
*/
void xr_define_local(CompilerContext *ctx, Compiler *compiler, XrString *name) {
    if (compiler->local_count >= MAXREGS) {
        xr_compiler_error(ctx, compiler, "Too many local variables");
        return;
    }
    
    /* 分配寄存器 */
    int reg = xr_allocreg(ctx, compiler);
    
    /* 添加到局部变量列表 */
    Local *local = &compiler->locals[compiler->local_count++];
    local->name = name;
    local->reg = reg;
    local->depth = compiler->scope_depth;
    local->is_captured = false;
    
    /* 保留寄存器 */
    xr_reservereg(compiler);
}

/*
** 定义局部变量（使用指定的寄存器）
** 用于函数定义等需要精确控制寄存器的情况
*/
static void define_local_with_reg(CompilerContext *ctx, Compiler *compiler, XrString *name, int reg) {
    if (compiler->local_count >= MAXREGS) {
        xr_compiler_error(ctx, compiler, "Too many local variables");
        return;
    }
    
    /* 添加到局部变量列表 */
    Local *local = &compiler->locals[compiler->local_count++];
    local->name = name;
    local->reg = reg;
    local->depth = compiler->scope_depth;
    local->is_captured = false;
    
    /* 保留寄存器 */
    xr_reservereg(compiler);
}

/*
** 解析局部变量
*/
int xr_resolve_local(Compiler *compiler, XrString *name) {
    /* 从内向外查找 */
    for (int i = compiler->local_count - 1; i >= 0; i--) {
        Local *local = &compiler->locals[i];
        if (local->name == name || 
            (local->name != NULL && name != NULL && 
             strcmp(local->name->chars, name->chars) == 0)) {
            return local->reg;
        }
    }
    return -1;  /* 未找到 */
}

/*
** 添加upvalue到编译器
*/
static int add_upvalue(CompilerContext *ctx, Compiler *compiler, uint8_t index, bool is_local) {
    int upvalue_count = compiler->proto->sizeupvalues;
    
    /* 检查是否已存在 */
    for (int i = 0; i < upvalue_count; i++) {
        Upvalue *upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->is_local == is_local) {
            return i;
        }
    }
    
    /* 添加新upvalue */
    if (upvalue_count >= UINT8_MAX) {
        xr_compiler_error(ctx, compiler, "Too many upvalues");
        return 0;
    }
    
    compiler->upvalues[upvalue_count].index = index;
    compiler->upvalues[upvalue_count].is_local = is_local;
    
    return xr_bc_proto_add_upvalue(compiler->proto, index, is_local);
}

/*
** 解析upvalue
*/
int xr_resolve_upvalue(CompilerContext *ctx, Compiler *compiler, XrString *name) {
    if (compiler->enclosing == NULL) {
        return -1;  /* 顶层函数，没有upvalue */
    }
    
    /* 在外层编译器中查找局部变量 */
    int local = xr_resolve_local(compiler->enclosing, name);
    if (local != -1) {
        /* 标记为被捕获 */
        compiler->enclosing->locals[local].is_captured = true;
        return add_upvalue(ctx, compiler, (uint8_t)local, true);
    }
    
    /* 在外层upvalue中查找 */
    int upvalue = xr_resolve_upvalue(ctx, compiler->enclosing, name);
    if (upvalue != -1) {
        return add_upvalue(ctx, compiler, (uint8_t)upvalue, false);
    }
    
    return -1;  /* 未找到 */
}

/* ========== 编译器初始化 ========== */

/*
** 初始化编译器
*/
void xr_compiler_init(CompilerContext *ctx, Compiler *compiler, FunctionType type) {
    compiler->enclosing = ctx->current;
    compiler->proto = xr_bc_proto_new();
    compiler->type = type;
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    compiler->loop_depth = 0;
    compiler->loop_start = 0;
    compiler->loop_scope = 0;
    compiler->had_error = false;
    compiler->panic_mode = false;
    
    /* 初始化寄存器状态 */
    compiler->rs.freereg = 0;
    compiler->rs.nactvar = 0;
    
    /* 设置为当前编译器 */
    ctx->current = compiler;
}

/*
** 结束编译
*/
Proto *xr_compiler_end(CompilerContext *ctx, Compiler *compiler) {
    /* 发射返回指令 */
    xr_emit_ABC(ctx, compiler, OP_RETURN, 0, 0, 0);
    
    Proto *proto = compiler->proto;
    
    /* 恢复外层编译器 */
    ctx->current = compiler->enclosing;
    
    /* 如果有错误，释放proto */
    if (compiler->had_error) {
        xr_bc_proto_free(proto);
        return NULL;
    }
    
    /* ===== 编译器优化 (v0.16.0) ===== */
    
    /* Peephole优化：跳转链消除、死代码删除等 */
    xr_peephole_optimize(proto);
    
    /* 指令融合优化：
     * 注意：LOADK→LOADI融合已在xfusion.c中禁用
     * 原因：测试发现性能下降5-10%
     * 其他融合（算术立即数等）仍然启用
     */
    xr_fusion_optimize(proto);
    
    /* 内联分析（只分析，不修改代码） */
    xr_inline_mark_candidates(proto);
    
    return proto;
}

/* ========== 表达式编译 ========== */

/*
** 编译字面量表达式
*/
static int compile_literal(CompilerContext *ctx, Compiler *compiler, LiteralNode *node) {
    int reg = xr_allocreg(ctx, compiler);
    
    XrType type = xr_value_type(node->value);
    switch (type) {
        case XR_TNULL:
            xr_emit_ABC(ctx, compiler, OP_LOADNIL, reg, 0, 0);
            break;
        
        case XR_TBOOL:
            if (xr_tobool(node->value)) {
                xr_emit_ABC(ctx, compiler, OP_LOADTRUE, reg, 0, 0);
            } else {
                xr_emit_ABC(ctx, compiler, OP_LOADFALSE, reg, 0, 0);
            }
            break;
        
        case XR_TINT: {
            xr_Integer ival = xr_toint(node->value);
            /* 尝试使用立即数指令 */
            if (ival >= -MAXARG_sBx && ival <= MAXARG_sBx) {
                xr_emit_AsBx(ctx, compiler, OP_LOADI, reg, (int)ival);
            } else {
                /* 使用常量池 */
                int kidx = xr_bc_proto_add_constant(compiler->proto, node->value);
                xr_emit_ABx(ctx, compiler, OP_LOADK, reg, kidx);
            }
            break;
        }
        
        case XR_TFLOAT: {
            /* 浮点数总是使用常量池 */
            int kidx = xr_bc_proto_add_constant(compiler->proto, node->value);
            xr_emit_ABx(ctx, compiler, OP_LOADK, reg, kidx);
            break;
        }
        
        case XR_TSTRING: {
            int kidx = xr_bc_proto_add_constant(compiler->proto, node->value);
            xr_emit_ABx(ctx, compiler, OP_LOADK, reg, kidx);
            break;
        }
        
        default:
            xr_compiler_error(ctx, compiler, "Unsupported literal type");
            break;
    }
    
    return reg;
}

/*
** 编译逻辑与运算（短路求值）
*/
static int compile_and(CompilerContext *ctx, Compiler *compiler, BinaryNode *node) {
    /* 编译左操作数 */
    int rb = xr_compile_expression(ctx, compiler, node->left);
    
    /* if not left then skip right */
    xr_emit_ABC(ctx, compiler, OP_TESTSET, rb, rb, 1);  /* 如果false则跳过 */
    int jump = xr_emit_jump(ctx, compiler, OP_JMP);
    
    /* 编译右操作数 */
    int rc = xr_compile_expression(ctx, compiler, node->right);
    
    /* 移动结果 */
    xr_emit_ABC(ctx, compiler, OP_MOVE, rb, rc, 0);
    xr_freereg(compiler, rc);
    
    /* 回填跳转 */
    xr_patch_jump(ctx, compiler, jump);
    
    return rb;
}

/*
** 编译逻辑或运算（短路求值）
*/
static int compile_or(CompilerContext *ctx, Compiler *compiler, BinaryNode *node) {
    /* 编译左操作数 */
    int rb = xr_compile_expression(ctx, compiler, node->left);
    
    /* if left then skip right */
    xr_emit_ABC(ctx, compiler, OP_TESTSET, rb, rb, 0);  /* 如果true则跳过 */
    int jump = xr_emit_jump(ctx, compiler, OP_JMP);
    
    /* 编译右操作数 */
    int rc = xr_compile_expression(ctx, compiler, node->right);
    
    /* 移动结果 */
    xr_emit_ABC(ctx, compiler, OP_MOVE, rb, rc, 0);
    xr_freereg(compiler, rc);
    
    /* 回填跳转 */
    xr_patch_jump(ctx, compiler, jump);
    
    return rb;
}

/*
** 编译二元运算表达式
*/
static int compile_binary(CompilerContext *ctx, Compiler *compiler, BinaryNode *node, AstNodeType type) {
    /* 逻辑运算需要短路求值 */
    if (type == AST_BINARY_AND) {
        return compile_and(ctx, compiler, node);
    }
    if (type == AST_BINARY_OR) {
        return compile_or(ctx, compiler, node);
    }
    
    /* ===== 优化1：常量折叠 (v0.15.0) ===== */
    /* 如果两个操作数都是字面量，尝试在编译时计算结果 */
    if ((node->left->type == AST_LITERAL_INT || node->left->type == AST_LITERAL_FLOAT) &&
        (node->right->type == AST_LITERAL_INT || node->right->type == AST_LITERAL_FLOAT)) {
        
        LiteralNode *left_lit = (LiteralNode *)&node->left->as;
        LiteralNode *right_lit = (LiteralNode *)&node->right->as;
        
        /* 映射AST节点类型到Token类型 */
        TokenType op_token;
        switch (type) {
            case AST_BINARY_ADD: op_token = TK_PLUS; break;
            case AST_BINARY_SUB: op_token = TK_MINUS; break;
            case AST_BINARY_MUL: op_token = TK_STAR; break;
            case AST_BINARY_DIV: op_token = TK_SLASH; break;
            case AST_BINARY_MOD: op_token = TK_PERCENT; break;
            default: op_token = TK_EOF; break;  /* 不支持的运算符 */
        }
        
        XrValue result;
        if (op_token != TK_EOF && 
            xr_opt_fold_binary(op_token, left_lit->value, right_lit->value, &result)) {
            /* 折叠成功！直接生成常量加载指令 */
            int dst = xr_allocreg(ctx, compiler);
            int kidx = xr_bc_proto_add_constant(compiler->proto, result);
            xr_emit_ABx(ctx, compiler, OP_LOADK, dst, kidx);
            return dst;
        }
    }
    /* ===== 常量折叠结束 ===== */
    
    /* 优化：检查右操作数是否是小整数常量 */
    if (node->right->type == AST_LITERAL_INT) {
        LiteralNode *lit = (LiteralNode *)&node->right->as;
        xr_Integer value = xr_toint(lit->value);
        
        /* 如果是小整数（-128到127），使用立即数指令 */
        if (value >= -128 && value <= 127) {
            int rb = xr_compile_expression(ctx, compiler, node->left);
            int ra = xr_allocreg(ctx, compiler);
            
            /* 选择优化指令 */
            OpCode op;
            bool use_optimized = true;
            
            switch (type) {
                case AST_BINARY_ADD: op = OP_ADDI; break;
                case AST_BINARY_SUB: op = OP_SUBI; break;
                case AST_BINARY_MUL: op = OP_MULI; break;
                default:
                    use_optimized = false;
                    break;
            }
            
            if (use_optimized) {
                /* 发射优化指令（ABsC格式） */
                xr_emit_ABsC(ctx, compiler, op, ra, rb, (int)value);
                xr_freereg(compiler, rb);
                return ra;
            }
        }
    }
    
    /* 通用路径：编译两个操作数 */
    int rb = xr_compile_expression(ctx, compiler, node->left);
    int rc = xr_compile_expression(ctx, compiler, node->right);
    int ra = xr_allocreg(ctx, compiler);
    
    /* 根据节点类型选择指令 */
    OpCode op;
    switch (type) {
        /* 算术运算 */
        case AST_BINARY_ADD: op = OP_ADD; break;
        case AST_BINARY_SUB: op = OP_SUB; break;
        case AST_BINARY_MUL: op = OP_MUL; break;
        case AST_BINARY_DIV: op = OP_DIV; break;
        case AST_BINARY_MOD: op = OP_MOD; break;
        
        default:
            xr_compiler_error(ctx, compiler, "Unknown binary operator: %d", type);
            return ra;
    }
    
    /* 发射指令 */
    xr_emit_ABC(ctx, compiler, op, ra, rb, rc);
    
    /* 释放操作数寄存器 */
    xr_freereg(compiler, rb);
    xr_freereg(compiler, rc);
    
    return ra;
}

/*
** 编译比较运算（返回布尔值）
*/
static int compile_comparison(CompilerContext *ctx, Compiler *compiler, BinaryNode *node, AstNodeType type) {
    /* 编译左操作数 */
    int rb = xr_compile_expression(ctx, compiler, node->left);
    
    /* 编译右操作数 */
    int rc = xr_compile_expression(ctx, compiler, node->right);
    
    /* 分配目标寄存器 */
    int ra = xr_allocreg(ctx, compiler);
    
    /* 根据节点类型选择指令 */
    OpCode op;
    bool negate = false;
    
    switch (type) {
        case AST_BINARY_EQ: op = OP_EQ; break;
        case AST_BINARY_NE: op = OP_EQ; negate = true; break;
        case AST_BINARY_LT: op = OP_LT; break;
        case AST_BINARY_LE: op = OP_LE; break;
        case AST_BINARY_GT: op = OP_GT; break;
        case AST_BINARY_GE: op = OP_GE; break;
        
        default:
            xr_compiler_error(ctx, compiler, "Unknown comparison operator: %d", type);
            return ra;
    }
    
    /* 生成比较代码并返回布尔值
     * 
     * 策略：使用条件跳转来设置布尔值
     * 
     * 关键理解：OP_LT等指令格式是 if (result != k) then skip next
     * - k=1: false(0) != 1 → skip, true(1) != 1 → don't skip
     * 
     * 正确的布局（k=1）：
     *   CMP R[a] R[b] 1      ; false时跳过JMP
     *   JMP true_label       ; true时执行这个
     *   LOADFALSE R[result]  ; false时跳过JMP，执行这个
     *   JMP end
     *   true_label:
     *   LOADTRUE R[result]
     *   end:
     */
    
    /* 发射比较指令（k=1：false时跳过下一条指令） */
    xr_emit_ABC(ctx, compiler, op, rb, rc, 1);
    
    /* 比较为true时的跳转 */
    int true_jump = xr_emit_jump(ctx, compiler, OP_JMP);
    
    /* 比较为false：加载false（跳过了JMP才到这里） */
    if (negate) {
        xr_emit_ABC(ctx, compiler, OP_LOADTRUE, ra, 0, 0);
    } else {
        xr_emit_ABC(ctx, compiler, OP_LOADFALSE, ra, 0, 0);
    }
    
    /* 跳过true分支 */
    int end_jump = xr_emit_jump(ctx, compiler, OP_JMP);
    
    /* 回填true跳转 */
    xr_patch_jump(ctx, compiler, true_jump);
    
    /* 比较为true：加载true */
    if (negate) {
        xr_emit_ABC(ctx, compiler, OP_LOADFALSE, ra, 0, 0);
    } else {
        xr_emit_ABC(ctx, compiler, OP_LOADTRUE, ra, 0, 0);
    }
    
    /* 回填end跳转 */
    xr_patch_jump(ctx, compiler, end_jump);
    
    /* 释放操作数寄存器 */
    xr_freereg(compiler, rb);
    xr_freereg(compiler, rc);
    
    return ra;
}

/*
** 编译一元运算表达式
*/
static int compile_unary(CompilerContext *ctx, Compiler *compiler, UnaryNode *node, AstNodeType type) {
    /* ===== 优化：常量折叠 (v0.15.0) ===== */
    /* 如果操作数是字面量，尝试在编译时计算结果 */
    if (node->operand->type == AST_LITERAL_INT || 
        node->operand->type == AST_LITERAL_FLOAT ||
        node->operand->type == AST_LITERAL_TRUE ||
        node->operand->type == AST_LITERAL_FALSE ||
        node->operand->type == AST_LITERAL_NULL) {
        
        LiteralNode *lit = (LiteralNode *)&node->operand->as;
        
        /* 映射AST节点类型到Token类型 */
        TokenType op_token;
        switch (type) {
            case AST_UNARY_NEG: op_token = TK_MINUS; break;
            case AST_UNARY_NOT: op_token = TK_NOT; break;
            default: op_token = TK_EOF; break;
        }
        
        XrValue result;
        if (op_token != TK_EOF && 
            xr_opt_fold_unary(op_token, lit->value, &result)) {
            /* 折叠成功！直接生成常量加载指令 */
            int dst = xr_allocreg(ctx, compiler);
            int kidx = xr_bc_proto_add_constant(compiler->proto, result);
            xr_emit_ABx(ctx, compiler, OP_LOADK, dst, kidx);
            return dst;
        }
    }
    /* ===== 常量折叠结束 ===== */
    
    /* 编译操作数 */
    int rb = xr_compile_expression(ctx, compiler, node->operand);
    
    /* 分配目标寄存器 */
    int ra = xr_allocreg(ctx, compiler);
    
    /* 根据节点类型选择指令 */
    OpCode op;
    switch (type) {
        case AST_UNARY_NEG: op = OP_UNM; break;
        case AST_UNARY_NOT: op = OP_NOT; break;
        default:
            xr_compiler_error(ctx, compiler, "Unknown unary operator: %d", type);
            return ra;
    }
    
    /* 发射指令 */
    xr_emit_ABC(ctx, compiler, op, ra, rb, 0);
    
    /* 释放操作数寄存器 */
    xr_freereg(compiler, rb);
    
    return ra;
}

/*
** 编译变量访问表达式
*/
static int compile_variable(CompilerContext *ctx, Compiler *compiler, VariableNode *node) {
    /* 创建临时字符串用于查找 */
    XrString *name_str = xr_string_new(node->name, strlen(node->name));
    
    /* 查找局部变量 */
    int reg = xr_resolve_local(compiler, name_str);
    if (reg >= 0) {
        return reg;  /* 局部变量，直接返回其寄存器 */
    }
    
    /* 查找upvalue */
    int upvalue = xr_resolve_upvalue(ctx, compiler, name_str);
    if (upvalue >= 0) {
        int ra = xr_allocreg(ctx, compiler);
        xr_emit_ABC(ctx, compiler, OP_GETUPVAL, ra, upvalue, 0);
        return ra;
    }
    
    /* 全局变量（Wren风格：使用固定索引） */
    int ra = xr_allocreg(ctx, compiler);
    int global_index = get_or_add_global(ctx, compiler, name_str);
    xr_emit_ABx(ctx, compiler, OP_GETGLOBAL, ra, global_index);  /* 索引而非常量 */
    return ra;
}

/*
** 编译表达式（主入口）
*/
int xr_compile_expression(CompilerContext *ctx, Compiler *compiler, AstNode *node) {
    if (node == NULL) {
        xr_compiler_error(ctx, compiler, "NULL expression node");
        return xr_allocreg(ctx, compiler);
    }
    
    switch (node->type) {
        /* 字面量 */
        case AST_LITERAL_INT:
        case AST_LITERAL_FLOAT:
        case AST_LITERAL_STRING:
        case AST_LITERAL_NULL:
        case AST_LITERAL_TRUE:
        case AST_LITERAL_FALSE:
            return compile_literal(ctx, compiler, (LiteralNode *)&node->as);
        
        /* 二元运算 */
        /* 算术运算 */
        case AST_BINARY_ADD:
        case AST_BINARY_SUB:
        case AST_BINARY_MUL:
        case AST_BINARY_DIV:
        case AST_BINARY_MOD:
            return compile_binary(ctx, compiler, (BinaryNode *)&node->as, node->type);
        
        /* 比较运算（返回布尔值） */
        case AST_BINARY_EQ:
        case AST_BINARY_NE:
        case AST_BINARY_LT:
        case AST_BINARY_LE:
        case AST_BINARY_GT:
        case AST_BINARY_GE:
            return compile_comparison(ctx, compiler, (BinaryNode *)&node->as, node->type);
        
        /* 逻辑运算（短路求值） */
        case AST_BINARY_AND:
        case AST_BINARY_OR:
            return compile_binary(ctx, compiler, (BinaryNode *)&node->as, node->type);
        
        /* 一元运算 */
        case AST_UNARY_NEG:
        case AST_UNARY_NOT:
            return compile_unary(ctx, compiler, (UnaryNode *)&node->as, node->type);
        
        /* 变量引用 */
        case AST_VARIABLE:
            return compile_variable(ctx, compiler, (VariableNode *)&node->as);
        
        /* 分组表达式 */
        case AST_GROUPING:
            return xr_compile_expression(ctx, compiler, node->as.grouping);
        
        /* 赋值表达式（作为表达式使用时） */
        case AST_ASSIGNMENT: {
            compile_assignment(ctx, compiler, &node->as.assignment);
            /* 赋值返回值：当前不返回，符合语句定义 */
            return xr_allocreg(ctx, compiler);
        }
        
        /* 函数调用 */
        case AST_CALL_EXPR:
            return compile_call(ctx, compiler, (CallExprNode *)&node->as);
        
        /* 数组操作 */
        case AST_ARRAY_LITERAL:
            return compile_array_literal(ctx, compiler, &node->as.array_literal);
        
        case AST_INDEX_GET:
            return compile_index_get(ctx, compiler, &node->as.index_get);
        
        /* v0.19.0：OOP表达式 */
        case AST_NEW_EXPR:
            return compile_new_expr(ctx, compiler, &node->as.new_expr);
        
        case AST_MEMBER_ACCESS:
            return compile_member_access(ctx, compiler, &node->as.member_access);
        
        case AST_THIS_EXPR:
            /* this表达式：返回寄存器0（this总是在寄存器0） */
            return 0;
        
        default:
            xr_compiler_error(ctx, compiler, "Unsupported expression type: %d (%s at line %d)", 
                            node->type, 
                            (node->type == 41 ? "AST_BINARY_EQ?" : "Unknown"),
                            node->line);
            return xr_allocreg(ctx, compiler);
    }
}

/* ========== 语句编译 ========== */

/*
** 编译表达式语句
*/
static void compile_expr_stmt(CompilerContext *ctx, Compiler *compiler, AstNode *expr) {
    /* 特殊处理赋值语句（它们不是表达式） */
    if (expr->type == AST_ASSIGNMENT) {
        compile_assignment(ctx, compiler, &expr->as.assignment);
        return;
    }
    
    /* 特殊处理成员赋值语句 */
    if (expr->type == AST_MEMBER_SET) {
        compile_member_set(ctx, compiler, &expr->as.member_set);
        return;
    }
    
    /* 特殊处理索引赋值语句 */
    if (expr->type == AST_INDEX_SET) {
        compile_index_set(ctx, compiler, &expr->as.index_set);
        return;
    }
    
    /* 其他表达式语句 */
    int reg = xr_compile_expression(ctx, compiler, expr);
    xr_freereg(compiler, reg);
}

/*
** 编译变量声明
*/
static void compile_var_decl(CompilerContext *ctx, Compiler *compiler, VarDeclNode *node) {
    /* 创建字符串 */
    XrString *name_str = xr_string_new(node->name, strlen(node->name));
    
    if (compiler->scope_depth == 0) {
        /* 全局变量（Wren风格：使用固定索引） */
        int reg = xr_compile_expression(ctx, compiler, node->initializer);
        
        int global_index = get_or_add_global(ctx, compiler, name_str);
        xr_emit_ABx(ctx, compiler, OP_SETGLOBAL, reg, global_index);  /* 索引而非常量 */
        
        xr_freereg(compiler, reg);
    } else {
        /* 局部变量 */
        /* 先定义变量（分配寄存器） */
        xr_define_local(ctx, compiler, name_str);
        
        /* 编译初始化表达式到该寄存器 */
        int local_reg = compiler->locals[compiler->local_count - 1].reg;
        int expr_reg = xr_compile_expression(ctx, compiler, node->initializer);
        
        /* 如果表达式结果不在目标寄存器，移动它 */
        if (expr_reg != local_reg) {
            xr_emit_ABC(ctx, compiler, OP_MOVE, local_reg, expr_reg, 0);
            xr_freereg(compiler, expr_reg);
        }
    }
}

/*
** 编译print语句
*/
static void compile_print(CompilerContext *ctx, Compiler *compiler, PrintNode *node) {
    /* 编译表达式 */
    int reg = xr_compile_expression(ctx, compiler, node->expr);
    
    /* 发射打印指令 */
    xr_emit_ABC(ctx, compiler, OP_PRINT, reg, 0, 0);
    
    xr_freereg(compiler, reg);
}

/*
** 编译赋值语句
*/
static void compile_assignment(CompilerContext *ctx, Compiler *compiler, AssignmentNode *node) {
    /* 创建字符串 */
    XrString *name_str = xr_string_new(node->name, strlen(node->name));
    
    /* 编译右值 */
    int value_reg = xr_compile_expression(ctx, compiler, node->value);
    
    /* 查找变量 */
    int local = xr_resolve_local(compiler, name_str);
    if (local >= 0) {
        /* 局部变量赋值 */
        if (value_reg != local) {
            xr_emit_ABC(ctx, compiler, OP_MOVE, local, value_reg, 0);
            xr_freereg(compiler, value_reg);
        }
    } else {
        int upvalue = xr_resolve_upvalue(ctx, compiler, name_str);
        if (upvalue >= 0) {
            /* Upvalue赋值 */
            xr_emit_ABC(ctx, compiler, OP_SETUPVAL, value_reg, upvalue, 0);
            xr_freereg(compiler, value_reg);
        } else {
            /* 全局变量赋值（Wren风格：使用固定索引） */
            int global_index = get_or_add_global(ctx, compiler, name_str);
            xr_emit_ABx(ctx, compiler, OP_SETGLOBAL, value_reg, global_index);  /* 索引而非常量 */
            xr_freereg(compiler, value_reg);
        }
    }
}

/*
** 编译if语句
*/
static void compile_if(CompilerContext *ctx, Compiler *compiler, IfStmtNode *node) {
    /* ⚡ 优化：直接编译比较表达式的条件跳转，避免生成中间布尔值 */
    AstNodeType cond_type = node->condition->type;
    
    if (cond_type == AST_BINARY_LE || cond_type == AST_BINARY_LT ||
        cond_type == AST_BINARY_GT || cond_type == AST_BINARY_GE ||
        cond_type == AST_BINARY_EQ || cond_type == AST_BINARY_NE) {
        
        /* 直接编译比较并条件跳转 */
        BinaryNode *cmp = (BinaryNode *)&node->condition->as;
        int rb = xr_compile_expression(ctx, compiler, cmp->left);
        int rc = xr_compile_expression(ctx, compiler, cmp->right);
        
        /* 选择比较指令 */
        OpCode op;
        int k = 0;  /* k=0: 比较为true时跳过下一条JMP（即执行then） */
        
        switch (cond_type) {
            case AST_BINARY_EQ: op = OP_EQ; break;
            case AST_BINARY_NE: op = OP_EQ; k = 1; break;  /* 反转 */
            case AST_BINARY_LT: op = OP_LT; break;
            case AST_BINARY_LE: op = OP_LE; break;
            case AST_BINARY_GT: op = OP_GT; break;
            case AST_BINARY_GE: op = OP_GE; break;
            default: op = OP_LT; break;
        }
        
        /* 发射比较指令：if (rb CMP rc) == true then skip JMP */
        xr_emit_ABC(ctx, compiler, op, rb, rc, k);
        int else_jump = xr_emit_jump(ctx, compiler, OP_JMP);
        
        xr_freereg(compiler, rb);
        xr_freereg(compiler, rc);
        
        /* 编译then分支 */
        xr_compile_statement(ctx, compiler, node->then_branch);
        
        /* 如果有else，跳过else分支 */
        int end_jump = -1;
        if (node->else_branch != NULL) {
            end_jump = xr_emit_jump(ctx, compiler, OP_JMP);
        }
        
        /* 回填else跳转（跳到else或end） */
        xr_patch_jump(ctx, compiler, else_jump);
        
        /* 编译else分支 */
        if (node->else_branch != NULL) {
            xr_compile_statement(ctx, compiler, node->else_branch);
            xr_patch_jump(ctx, compiler, end_jump);
        }
    } else {
        /* 普通表达式：编译为布尔值再测试 */
        int cond_reg = xr_compile_expression(ctx, compiler, node->condition);
        
        xr_emit_ABC(ctx, compiler, OP_TEST, cond_reg, 0, 0);
        int then_jump = xr_emit_jump(ctx, compiler, OP_JMP);
        xr_freereg(compiler, cond_reg);
        
        /* 编译then分支 */
        xr_compile_statement(ctx, compiler, node->then_branch);
        
        /* 跳过else分支 */
        int else_jump = xr_emit_jump(ctx, compiler, OP_JMP);
        
        /* 回填then_jump */
        xr_patch_jump(ctx, compiler, then_jump);
        
        /* 编译else分支（如果有） */
        if (node->else_branch != NULL) {
            xr_compile_statement(ctx, compiler, node->else_branch);
        }
        
        /* 回填else_jump */
        xr_patch_jump(ctx, compiler, else_jump);
    }
}

/*
** 编译while循环
*/
static void compile_while(CompilerContext *ctx, Compiler *compiler, WhileStmtNode *node) {
    int loop_start = compiler->proto->sizecode;
    
    /* 编译条件 */
    int cond_reg = xr_compile_expression(ctx, compiler, node->condition);
    
    /* if not cond then exit loop */
    xr_emit_ABC(ctx, compiler, OP_TEST, cond_reg, 1, 0);
    int exit_jump = xr_emit_jump(ctx, compiler, OP_JMP);
    xr_freereg(compiler, cond_reg);
    
    /* 编译循环体 */
    compiler->loop_depth++;
    compiler->loop_start = loop_start;
    xr_compile_statement(ctx, compiler, node->body);
    compiler->loop_depth--;
    
    /* 跳回循环开始 */
    xr_emit_loop(ctx, compiler, loop_start);
    
    /* 回填退出跳转 */
    xr_patch_jump(ctx, compiler, exit_jump);
}

/*
** 编译for循环
*/
static void compile_for(CompilerContext *ctx, Compiler *compiler, ForStmtNode *node) {
    /* 进入循环作用域 */
    xr_begin_scope(compiler);
    
    /* 编译初始化 */
    if (node->initializer != NULL) {
        xr_compile_statement(ctx, compiler, node->initializer);
    }
    
    int loop_start = compiler->proto->sizecode;
    
    /* 编译条件 */
    int exit_jump = -1;
    if (node->condition != NULL) {
        int cond_reg = xr_compile_expression(ctx, compiler, node->condition);
        /* OP_TEST k=0: 如果条件为false则跳过下一条指令（跳过JMP，继续循环）*/
        /* 我们希望false时跳出循环，所以需要k=0，然后JMP跳出 */
        /* 但实际逻辑是：TEST如果false跳过JMP，我们想false时跳出，所以不应该跳过JMP */
        /* 正确的逻辑：k=0表示false时跳过，我们要false时跳出，所以k应该是0，然后反转JMP逻辑 */
        /* 简化：TEST k=0 + JMP表示"false时跳过JMP继续，true时执行JMP跳出" */
        /* 这正好相反！我们要"false时跳出，true时继续" */
        /* 所以应该用k=0，但是不跳过JMP，而是跳过继续的代码 */
        /* 让我换个思路：if (cond) { body } 应该是 TEST cond 0; JMP exit; body; exit: */
        xr_emit_ABC(ctx, compiler, OP_TEST, cond_reg, 0, 0);
        exit_jump = xr_emit_jump(ctx, compiler, OP_JMP);
        xr_freereg(compiler, cond_reg);
    }
    
    /* 编译循环体 */
    compiler->loop_depth++;
    compiler->loop_start = loop_start;
    xr_compile_statement(ctx, compiler, node->body);
    compiler->loop_depth--;
    
    /* 编译更新表达式 */
    if (node->increment != NULL) {
        int inc_reg = xr_compile_expression(ctx, compiler, node->increment);
        xr_freereg(compiler, inc_reg);
    }
    
    /* 跳回循环开始 */
    xr_emit_loop(ctx, compiler, loop_start);
    
    /* 回填退出跳转 */
    if (exit_jump != -1) {
        xr_patch_jump(ctx, compiler, exit_jump);
    }
    
    /* 退出循环作用域 */
    xr_end_scope(ctx, compiler);
}

/*
** 编译函数定义
*/
static void compile_function(CompilerContext *ctx, Compiler *compiler, FunctionDeclNode *node) {
    /* 如果是命名局部函数，先在当前作用域定义函数名 */
    /* 这样函数体编译时可以通过upvalue递归访问自己 */
    int func_reg = -1;
    XrString *name_str = NULL;
    
    if (node->name != NULL && compiler->scope_depth > 0) {
        /* 局部函数：先分配寄存器并定义变量（值暂时为nil） */
        name_str = xr_string_new(node->name, strlen(node->name));
        func_reg = xr_allocreg(ctx, compiler);
        define_local_with_reg(ctx, compiler, name_str, func_reg);
    }
    
    /* 创建新的编译器（嵌套） */
    Compiler function_compiler;
    xr_compiler_init(ctx, &function_compiler, FUNCTION_FUNCTION);
    function_compiler.enclosing = compiler;
    
    /* 设置函数名 */
    if (node->name != NULL) {
        if (name_str == NULL) {
            name_str = xr_string_new(node->name, strlen(node->name));
        }
        function_compiler.proto->name = name_str;
    }
    
    /* 设置参数数量 */
    function_compiler.proto->numparams = node->param_count;
    
    /* 进入函数作用域 */
    xr_begin_scope(&function_compiler);
    
    /* 定义参数为局部变量 */
    for (int i = 0; i < node->param_count; i++) {
        XrString *param_str = xr_string_new(node->parameters[i], strlen(node->parameters[i]));
        xr_define_local(ctx, &function_compiler, param_str);
    }
    
    /* 编译函数体 */
    xr_compile_statement(ctx, &function_compiler, node->body);
    
    /* 结束编译 */
    Proto *proto = xr_compiler_end(ctx, &function_compiler);
    
    if (proto != NULL) {
        /* 将函数原型添加到父编译器 */
        int proto_idx = xr_bc_proto_add_proto(compiler->proto, proto);
        
        /* 如果是命名函数，定义为变量 */
        if (node->name != NULL) {
            if (compiler->scope_depth == 0) {
                /* 全局函数（Wren风格：使用固定索引） */
                int reg = xr_allocreg(ctx, compiler);
                xr_emit_ABx(ctx, compiler, OP_CLOSURE, reg, proto_idx);
                
                int global_index = get_or_add_global(ctx, compiler, name_str);
                xr_emit_ABx(ctx, compiler, OP_SETGLOBAL, reg, global_index);  /* 索引而非常量 */
                xr_freereg(compiler, reg);
            } else {
                /* 局部函数 - 使用之前分配的寄存器，现在填充闭包 */
                xr_emit_ABx(ctx, compiler, OP_CLOSURE, func_reg, proto_idx);
                /* 函数名已经在前面定义，不需要再定义 */
            }
        } else {
            /* 匿名函数表达式 */
            int reg = xr_allocreg(ctx, compiler);
            xr_emit_ABx(ctx, compiler, OP_CLOSURE, reg, proto_idx);
            /* 不定义变量，返回寄存器 */
        }
    }
}

/*
** 编译函数调用
** is_tail: 是否是尾调用位置（Phase 2新增）
*/
static int compile_call_internal(CompilerContext *ctx, Compiler *compiler, CallExprNode *node, bool is_tail) {
    /* ⭐ v0.19.0：检测方法调用模式 obj.method() */
    if (node->callee->type == AST_MEMBER_ACCESS) {
        MemberAccessNode *member = &node->callee->as.member_access;
        
        /* 编译对象表达式 */
        int obj_reg = xr_compile_expression(ctx, compiler, member->object);
        
        /* 编译参数到连续寄存器 */
        for (int i = 0; i < node->arg_count; i++) {
            int arg_reg = xr_compile_expression(ctx, compiler, node->arguments[i]);
            if (arg_reg != obj_reg + i + 1) {
                xr_emit_ABC(ctx, compiler, OP_MOVE, obj_reg + i + 1, arg_reg, 0);
                xr_freereg(compiler, arg_reg);
            }
        }
        
        /* v0.20.0: 使用Symbol代替方法名字符串 */
        int method_symbol = symbol_get_or_create(global_method_symbols, member->name);
        
        /* 生成OP_INVOKE指令 */
        /* 注意：现在B参数是symbol，不再是常量索引 */
        xr_emit_ABC(ctx, compiler, OP_INVOKE, obj_reg, method_symbol, node->arg_count);
        
        /* 返回值在obj_reg */
        return obj_reg;
    }
    
    /* ⭐ v0.16.0优化：检测递归调用模式 */
    bool is_recursive = false;
    
    /* 检查callee是否是对当前函数的引用 */
    if (node->callee->type == AST_VARIABLE && 
        compiler->proto->name && 
        compiler->type == FUNCTION_FUNCTION) {
        
        VariableNode *var = (VariableNode *)&node->callee->as;
        /* 如果调用的是与当前函数同名的全局变量，则是递归调用 */
        if (strcmp(var->name, compiler->proto->name->chars) == 0) {
            is_recursive = true;
        }
    }
    
    int func_reg;
    
    if (is_recursive) {
        /* 递归调用：直接分配参数寄存器，无需GETGLOBAL */
        func_reg = xr_allocreg(ctx, compiler);
    } else {
        /* 普通调用：编译被调用的函数表达式 */
        func_reg = xr_compile_expression(ctx, compiler, node->callee);
    }
    
    /* 编译参数 */
    for (int i = 0; i < node->arg_count; i++) {
        int arg_reg = xr_compile_expression(ctx, compiler, node->arguments[i]);
        /* 参数应该在连续的寄存器中 */
        if (arg_reg != func_reg + i + 1) {
            xr_emit_ABC(ctx, compiler, OP_MOVE, func_reg + i + 1, arg_reg, 0);
            xr_freereg(compiler, arg_reg);
        }
    }
    
    /* ⭐ 根据递归和尾调用情况选择指令 */
    if (is_recursive) {
        /* 递归调用优化：使用CALLSELF */
        xr_emit_ABC(ctx, compiler, OP_CALLSELF, func_reg, node->arg_count, 1);
        return func_reg;
    } else if (is_tail && compiler->type == FUNCTION_FUNCTION) {
        /* 尾调用：复用栈帧，不增加调用深度 */
        xr_emit_ABC(ctx, compiler, OP_TAILCALL, func_reg, node->arg_count, 0);
        /* 尾调用后不返回值到寄存器（直接返回） */
        return -1;  /* 特殊标记：无返回寄存器 */
    } else {
        /* 普通调用 */
        xr_emit_ABC(ctx, compiler, OP_CALL, func_reg, node->arg_count, 1);
        /* 结果在func_reg中 */
        return func_reg;
    }
}

/*
** 编译函数调用
*/
static int compile_call(CompilerContext *ctx, Compiler *compiler, CallExprNode *node) {
    return compile_call_internal(ctx, compiler, node, false);
}

/*
** 编译return语句
** ⭐ Phase 2: 支持尾调用优化
*/
static void compile_return(CompilerContext *ctx, Compiler *compiler, ReturnStmtNode *node) {
    if (compiler->type == FUNCTION_SCRIPT) {
        xr_compiler_error(ctx, compiler, "Cannot return from top-level code");
        return;
    }
    
    if (node->value != NULL) {
        /* ⭐ Phase 2: 检测尾调用
        ** 如果return的表达式是函数调用，这就是尾调用位置
        */
        if (node->value->type == AST_CALL_EXPR) {
            /* 尾调用优化：使用OP_TAILCALL代替OP_CALL+OP_RETURN */
            compile_call_internal(ctx, compiler, (CallExprNode *)&node->value->as, true);
            /* TAILCALL指令已经包含了return语义，无需额外的RETURN指令 */
        } else {
            /* 普通return：先计算表达式，再返回 */
            int reg = xr_compile_expression(ctx, compiler, node->value);
            xr_emit_ABC(ctx, compiler, OP_RETURN, reg, 1, 0);
            xr_freereg(compiler, reg);
        }
    } else {
        /* 返回null */
        xr_emit_ABC(ctx, compiler, OP_RETURN, 0, 0, 0);
    }
}

/*
** 编译数组字面量
*/
static int compile_array_literal(CompilerContext *ctx, Compiler *compiler, ArrayLiteralNode *node) {
    /* 分配目标寄存器 */
    int array_reg = xr_allocreg(ctx, compiler);
    
    /* 创建空数组 */
    /* NEWTABLE A B C: R[A] = {} (B=数组大小提示, C=哈希大小提示) */
    xr_emit_ABC(ctx, compiler, OP_NEWTABLE, array_reg, node->count, 0);
    
    /* 使用SETLIST批量设置元素 */
    if (node->count > 0) {
        /* 编译所有元素到连续寄存器 */
        for (int i = 0; i < node->count; i++) {
            int elem_reg = xr_compile_expression(ctx, compiler, node->elements[i]);
            
            /* 确保元素在连续寄存器中 */
            int target_reg = array_reg + i + 1;
            if (elem_reg != target_reg) {
                xr_emit_ABC(ctx, compiler, OP_MOVE, target_reg, elem_reg, 0);
                xr_freereg(compiler, elem_reg);
            }
        }
        
        /* SETLIST A B C: R[A][i] = R[A+i], 1 <= i <= B */
        xr_emit_ABC(ctx, compiler, OP_SETLIST, array_reg, node->count, 0);
        
        /* 释放临时寄存器 */
        for (int i = 0; i < node->count; i++) {
            xr_freereg(compiler, array_reg + i + 1);
        }
    }
    
    return array_reg;
}

/*
** 编译索引访问
*/
static int compile_index_get(CompilerContext *ctx, Compiler *compiler, IndexGetNode *node) {
    /* 编译数组表达式 */
    int array_reg = xr_compile_expression(ctx, compiler, node->array);
    
    /* 编译索引表达式 */
    int index_reg = xr_compile_expression(ctx, compiler, node->index);
    
    /* 分配结果寄存器 */
    int result_reg = xr_allocreg(ctx, compiler);
    
    /* 使用通用GETTABLE指令 */
    /* GETI优化：可在性能优化阶段添加 */
    xr_emit_ABC(ctx, compiler, OP_GETTABLE, result_reg, array_reg, index_reg);
    xr_freereg(compiler, index_reg);
    
    xr_freereg(compiler, array_reg);
    
    return result_reg;
}

/*
** 编译索引赋值
*/
static void compile_index_set(CompilerContext *ctx, Compiler *compiler, IndexSetNode *node) {
    /* 编译数组表达式 */
    int array_reg = xr_compile_expression(ctx, compiler, node->array);
    
    /* 编译索引表达式 */
    int index_reg = xr_compile_expression(ctx, compiler, node->index);
    
    /* 编译值表达式 */
    int value_reg = xr_compile_expression(ctx, compiler, node->value);
    
    /* 使用通用SETTABLE指令 */
    /* SETI优化：可在性能优化阶段添加 */
    xr_emit_ABC(ctx, compiler, OP_SETTABLE, array_reg, index_reg, value_reg);
    xr_freereg(compiler, index_reg);
    
    xr_freereg(compiler, value_reg);
    xr_freereg(compiler, array_reg);
}

/*
** 编译语句（主入口）
*/
void xr_compile_statement(CompilerContext *ctx, Compiler *compiler, AstNode *node) {
    if (node == NULL) {
        return;
    }
    
    /* 更新当前行号 */
    ctx->current_line = node->line;
    
    switch (node->type) {
        case AST_EXPR_STMT:
            compile_expr_stmt(ctx, compiler, node->as.expr_stmt);
            break;
        
        case AST_PRINT_STMT:
            compile_print(ctx, compiler, &node->as.print_stmt);
            break;
        
        case AST_VAR_DECL:
        case AST_CONST_DECL:
            compile_var_decl(ctx, compiler, &node->as.var_decl);
            break;
        
        case AST_ASSIGNMENT:
            compile_assignment(ctx, compiler, &node->as.assignment);
            break;
        
        case AST_IF_STMT:
            compile_if(ctx, compiler, &node->as.if_stmt);
            break;
        
        case AST_WHILE_STMT:
            compile_while(ctx, compiler, &node->as.while_stmt);
            break;
        
        case AST_FOR_STMT:
            compile_for(ctx, compiler, &node->as.for_stmt);
            break;
        
        case AST_FUNCTION_DECL:
            compile_function(ctx, compiler, &node->as.function_decl);
            break;
        
        case AST_RETURN_STMT:
            compile_return(ctx, compiler, &node->as.return_stmt);
            break;
        
        case AST_INDEX_SET:
            compile_index_set(ctx, compiler, &node->as.index_set);
            break;
        
        case AST_BLOCK: {
            BlockNode *block = &node->as.block;
            xr_begin_scope(compiler);
            for (int i = 0; i < block->count; i++) {
                xr_compile_statement(ctx, compiler, block->statements[i]);
            }
            xr_end_scope(ctx, compiler);
            break;
        }
        
        case AST_PROGRAM: {
            ProgramNode *program = &node->as.program;
            for (int i = 0; i < program->count; i++) {
                xr_compile_statement(ctx, compiler, program->statements[i]);
            }
            break;
        }
        
        /* v0.19.0：OOP相关节点 */
        case AST_CLASS_DECL:
            compile_class(ctx, compiler, &node->as.class_decl);
            break;
        
        case AST_MEMBER_SET:
            compile_member_set(ctx, compiler, &node->as.member_set);
            break;
        
        default:
            xr_compiler_error(ctx, compiler, "Unsupported statement type: %d", node->type);
            break;
    }
}

/* ========== 编译API ========== */

/* ========== 全局变量索引管理（Wren风格优化）========== */

/*
** 获取或添加全局变量索引
*/
static int get_or_add_global(CompilerContext *ctx, Compiler *compiler, XrString *name) {
    /* 查找是否已存在 */
    for (int i = 0; i < ctx->global_var_count; i++) {
        if (ctx->global_vars[i].name != NULL && 
            strcmp(ctx->global_vars[i].name->chars, name->chars) == 0) {
            return i;
        }
    }
    
    /* 添加新的全局变量 */
    if (ctx->global_var_count >= MAX_GLOBALS) {
        xr_compiler_error(ctx, compiler, "Too many global variables (max %d)", MAX_GLOBALS);
        return 0;
    }
    
    int index = ctx->global_var_count++;
    ctx->global_vars[index].name = name;
    ctx->global_vars[index].index = index;
    
    return index;
}

/*
** 编译AST到函数原型
*/
Proto *xr_compile(CompilerContext *ctx, AstNode *ast) {
    /* 重置全局变量计数（每次编译重新开始） */
    ctx->global_var_count = 0;
    for (int i = 0; i < MAX_GLOBALS; i++) {
        ctx->global_vars[i].name = NULL;
        ctx->global_vars[i].index = -1;
    }
    
    Compiler compiler;
    xr_compiler_init(ctx, &compiler, FUNCTION_SCRIPT);
    
    /* 设置全局变量数组指针 */
    compiler.globals = ctx->global_vars;
    compiler.global_count = &ctx->global_var_count;
    
    /* 编译AST */
    xr_compile_statement(ctx, &compiler, ast);
    
    /* 将全局变量数量保存到Proto（用于VM初始化） */
    compiler.proto->num_globals = ctx->global_var_count;
    
    /* 结束编译 */
    return xr_compiler_end(ctx, &compiler);
}

/* ========== OOP编译支持（v0.19.0新增）========== */

/*
** 编译类声明
** class Number { ... }
*/
static void compile_class(CompilerContext *ctx, Compiler *compiler, ClassDeclNode *node) {
    /* 创建类对象并存储为全局变量 */
    int class_reg = xr_allocreg(ctx, compiler);
    
    /* 生成 OP_CLASS 指令创建类对象 */
    XrString *class_name = xr_string_new(node->name, strlen(node->name));
    int name_idx = xr_bc_proto_add_constant(compiler->proto, XR_OBJ_TO_VAL(class_name));
    xr_emit_ABx(ctx, compiler, OP_CLASS, class_reg, name_idx);
    
    /* 处理字段声明 */
    for (int i = 0; i < node->field_count; i++) {
        AstNode *field_node = node->fields[i];
        
        if (field_node->type != AST_FIELD_DECL) {
            continue;
        }
        
        FieldDeclNode *field = &field_node->as.field_decl;
        
        /* 生成字段名常量 */
        XrString *field_name = xr_string_new(field->name, strlen(field->name));
        int field_name_idx = xr_bc_proto_add_constant(compiler->proto, XR_OBJ_TO_VAL(field_name));
        
        /* 生成类型名常量（如果有）*/
        int type_name_idx = 0;  /* 0表示无类型 */
        if (field->type_name != NULL) {
            XrString *type_name = xr_string_new(field->type_name, strlen(field->type_name));
            type_name_idx = xr_bc_proto_add_constant(compiler->proto, XR_OBJ_TO_VAL(type_name));
        }
        
        /* 发射 OP_ADDFIELD 指令 */
        xr_emit_ABC(ctx, compiler, OP_ADDFIELD, class_reg, field_name_idx, type_name_idx);
    }
    
    /* 如果有超类，设置继承 */
    if (node->super_name != NULL) {
        /* 加载超类 */
        XrString *super_name = xr_string_new(node->super_name, strlen(node->super_name));
        int super_global = get_or_add_global(ctx, compiler, super_name);
        int super_reg = xr_allocreg(ctx, compiler);
        xr_emit_ABx(ctx, compiler, OP_GETGLOBAL, super_reg, super_global);
        
        /* 设置继承关系 */
        xr_emit_ABC(ctx, compiler, OP_INHERIT, class_reg, super_reg, 0);
        xr_freereg(compiler, super_reg);
    }
    
    /* 编译所有方法 */
    for (int i = 0; i < node->method_count; i++) {
        AstNode *method_node = node->methods[i];
        
        if (method_node->type != AST_METHOD_DECL) {
            continue;
        }
        
        MethodDeclNode *method = &method_node->as.method_decl;
        
        /* 编译方法函数 */
        Compiler method_compiler;
        xr_compiler_init(ctx, &method_compiler, FUNCTION_FUNCTION);
        method_compiler.enclosing = compiler;
        
        /* 设置方法名 */
        XrString *method_name_str = xr_string_new(method->name, strlen(method->name));
        method_compiler.proto->name = method_name_str;
        
        /* 设置参数数量（this + 显式参数） */
        method_compiler.proto->numparams = method->param_count + 1;
        
        /* 参数0是this（隐式） */
        XrString *this_name = xr_string_new("this", 4);
        int this_reg = xr_allocreg(ctx, &method_compiler);
        define_local_with_reg(ctx, &method_compiler, this_name, this_reg);
        
        /* 添加显式参数 */
        for (int p = 0; p < method->param_count; p++) {
            XrString *param_name = xr_string_new(method->parameters[p], 
                                                strlen(method->parameters[p]));
            int param_reg = xr_allocreg(ctx, &method_compiler);
            define_local_with_reg(ctx, &method_compiler, param_name, param_reg);
        }
        
        /* 编译方法体 */
        xr_compile_statement(ctx, &method_compiler, method->body);
        
        /* 如果是构造函数，自动添加 return this */
        if (method->is_constructor || strcmp(method->name, "constructor") == 0) {
            /* this在寄存器0 */
            xr_emit_ABC(ctx, &method_compiler, OP_RETURN, 0, 1, 0);
        }
        
        /* 结束方法编译 */
        Proto *method_proto = xr_compiler_end(ctx, &method_compiler);
        
        if (method_proto != NULL) {
            /* 将方法添加到类 */
            int proto_idx = xr_bc_proto_add_proto(compiler->proto, method_proto);
            int method_reg = xr_allocreg(ctx, compiler);
            xr_emit_ABx(ctx, compiler, OP_CLOSURE, method_reg, proto_idx);
            
            /* v0.20.0: 使用Symbol代替方法名字符串 */
            int method_symbol = symbol_get_or_create(global_method_symbols, method->name);
            
            /* OP_METHOD: class_reg[method_symbol] = method_closure */
            /* 注意：现在B参数是symbol（整数），不再是常量索引 */
            xr_emit_ABC(ctx, compiler, OP_METHOD, class_reg, method_symbol, method_reg);
            xr_freereg(compiler, method_reg);
        }
    }
    
    /* 将类对象存储为全局变量 */
    int global_index = get_or_add_global(ctx, compiler, class_name);
    xr_emit_ABx(ctx, compiler, OP_SETGLOBAL, class_reg, global_index);
    xr_freereg(compiler, class_reg);
}

/*
** 编译new表达式
** new Number(10)
*/
static int compile_new_expr(CompilerContext *ctx, Compiler *compiler, NewExprNode *node) {
    /* 加载类对象 */
    XrString *class_name = xr_string_new(node->class_name, strlen(node->class_name));
    int global_index = get_or_add_global(ctx, compiler, class_name);
    int class_reg = xr_allocreg(ctx, compiler);
    xr_emit_ABx(ctx, compiler, OP_GETGLOBAL, class_reg, global_index);
    
    /* 编译构造参数 */
    for (int i = 0; i < node->arg_count; i++) {
        int arg_reg = xr_compile_expression(ctx, compiler, node->arguments[i]);
        /* 参数应该在连续的寄存器中 */
        if (arg_reg != class_reg + 1 + i) {
            /* 如果不连续，需要移动 */
            xr_emit_ABC(ctx, compiler, OP_MOVE, class_reg + 1 + i, arg_reg, 0);
            xr_freereg(compiler, arg_reg);
        }
    }
    
    /* 调用构造函数 */
    /* v0.20.0: 使用预定义的SYMBOL_CONSTRUCTOR */
    /* OP_INVOKE: class_reg.constructor(args) */
    int constructor_symbol = SYMBOL_CONSTRUCTOR;  /* 预定义symbol = 0 */
    
    /* INVOKE 指令：R[A] = R[B]:method(args) */
    /* A=结果寄存器, B=对象寄存器, C=参数数量 */
    /* 注意：现在B参数是symbol，不再是常量索引 */
    xr_emit_ABC(ctx, compiler, OP_INVOKE, class_reg, constructor_symbol, node->arg_count);
    
    return class_reg;  /* 返回实例所在的寄存器 */
}

/*
** 编译成员访问
** obj.field 或 obj.method
*/
static int compile_member_access(CompilerContext *ctx, Compiler *compiler, MemberAccessNode *node) {
    /* 编译对象表达式 */
    int obj_reg = xr_compile_expression(ctx, compiler, node->object);
    
    /* 生成属性访问指令 */
    XrString *prop_name = xr_string_new(node->name, strlen(node->name));
    int name_idx = xr_bc_proto_add_constant(compiler->proto, XR_OBJ_TO_VAL(prop_name));
    
    int result_reg = xr_allocreg(ctx, compiler);
    xr_emit_ABC(ctx, compiler, OP_GETPROP, result_reg, obj_reg, name_idx);
    
    xr_freereg(compiler, obj_reg);
    return result_reg;
}

/*
** 编译成员赋值
** obj.field = value
*/
static void compile_member_set(CompilerContext *ctx, Compiler *compiler, MemberSetNode *node) {
    /* 编译对象表达式 */
    int obj_reg = xr_compile_expression(ctx, compiler, node->object);
    
    /* 编译值表达式 */
    int value_reg = xr_compile_expression(ctx, compiler, node->value);
    
    /* 生成属性设置指令 */
    XrString *prop_name = xr_string_new(node->member, strlen(node->member));
    int name_idx = xr_bc_proto_add_constant(compiler->proto, XR_OBJ_TO_VAL(prop_name));
    
    xr_emit_ABC(ctx, compiler, OP_SETPROP, obj_reg, name_idx, value_reg);
    
    xr_freereg(compiler, value_reg);
    xr_freereg(compiler, obj_reg);
}


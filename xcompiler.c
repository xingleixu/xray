/*
** xcompiler.c
** Xray 寄存器编译器实现
*/

#include "xcompiler.h"
#include "xmem.h"
#include "xstring.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* 当前编译器（简化全局访问） */
static Compiler *current = NULL;

/* 当前行号（用于调试信息） */
static int current_line = 1;

/* 全局变量索引（Wren风格优化） */
static GlobalVar global_vars[MAX_GLOBALS];
static int global_var_count = 0;

/* ========== 前向声明 ========== */

static int get_or_add_global(Compiler *compiler, XrString *name);

static int compile_literal(Compiler *compiler, LiteralNode *node);
static int compile_binary(Compiler *compiler, BinaryNode *node, AstNodeType type);
static int compile_comparison(Compiler *compiler, BinaryNode *node, AstNodeType type);
static int compile_unary(Compiler *compiler, UnaryNode *node, AstNodeType type);
static int compile_variable(Compiler *compiler, VariableNode *node);
static void compile_assignment(Compiler *compiler, AssignmentNode *node);
static void compile_print(Compiler *compiler, PrintNode *node);
static void compile_if(Compiler *compiler, IfStmtNode *node);
static void compile_while(Compiler *compiler, WhileStmtNode *node);
static void compile_for(Compiler *compiler, ForStmtNode *node);
static int compile_call(Compiler *compiler, CallExprNode *node);
static void compile_function(Compiler *compiler, FunctionDeclNode *node);
static void compile_return(Compiler *compiler, ReturnStmtNode *node);
static int compile_array_literal(Compiler *compiler, ArrayLiteralNode *node);
static int compile_index_get(Compiler *compiler, IndexGetNode *node);
static void compile_index_set(Compiler *compiler, IndexSetNode *node);

/* ========== 错误处理 ========== */

/*
** 报告编译错误
*/
void xr_compiler_error(Compiler *compiler, const char *format, ...) {
    if (compiler->panic_mode) {
        return;  /* 避免错误雪崩 */
    }
    compiler->panic_mode = true;
    compiler->had_error = true;
    
    fprintf(stderr, "[line %d] Error: ", current_line);
    
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
int xr_allocreg(Compiler *compiler) {
    RegState *rs = &compiler->rs;
    if (rs->freereg >= MAXREGS) {
        xr_compiler_error(compiler, "Too many registers (max %d)", MAXREGS);
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
static void emit_instruction(Compiler *compiler, Instruction inst) {
    xr_bc_proto_write(compiler->proto, inst, current_line);
}

/*
** 发射ABC格式指令
*/
void xr_emit_ABC(Compiler *compiler, OpCode op, int a, int b, int c) {
    Instruction inst = CREATE_ABC(op, a, b, c);
    emit_instruction(compiler, inst);
}

/*
** 发射ABsC格式指令（B是寄存器，C是有符号立即数）
*/
void xr_emit_ABsC(Compiler *compiler, OpCode op, int a, int b, int sc) {
    /* 将有符号整数转换为无符号8位（模256） */
    uint8_t c = (uint8_t)(sc & 0xFF);
    Instruction inst = CREATE_ABC(op, a, b, c);
    emit_instruction(compiler, inst);
}

/*
** 发射ABx格式指令
*/
void xr_emit_ABx(Compiler *compiler, OpCode op, int a, int bx) {
    Instruction inst = CREATE_ABx(op, a, bx);
    emit_instruction(compiler, inst);
}

/*
** 发射AsBx格式指令
*/
void xr_emit_AsBx(Compiler *compiler, OpCode op, int a, int sbx) {
    Instruction inst = CREATE_AsBx(op, a, sbx);
    emit_instruction(compiler, inst);
}

/*
** 发射跳转指令（返回跳转位置）
*/
int xr_emit_jump(Compiler *compiler, OpCode op) {
    /* 发射占位符跳转指令 */
    xr_emit_ABC(compiler, op, 0, 0, 0);  /* 跳转偏移稍后回填 */
    return compiler->proto->sizecode - 1;
}

/*
** 回填跳转偏移
*/
void xr_patch_jump(Compiler *compiler, int offset) {
    /* 计算跳转距离 */
    int jump = compiler->proto->sizecode - offset - 1;
    
    if (jump > MAXARG_sJ) {
        xr_compiler_error(compiler, "Too much code to jump over");
    }
    
    /* 回填跳转指令 */
    Instruction *inst = &compiler->proto->code[offset];
    *inst = CREATE_sJ(GET_OPCODE(*inst), jump);
}

/*
** 发射循环指令（向后跳转）
*/
void xr_emit_loop(Compiler *compiler, int loop_start) {
    int offset = compiler->proto->sizecode - loop_start + 1;
    if (offset > MAXARG_sJ) {
        xr_compiler_error(compiler, "Loop body too large");
    }
    
    xr_emit_ABC(compiler, OP_JMP, 0, 0, 0);
    
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
void xr_end_scope(Compiler *compiler) {
    compiler->scope_depth--;
    
    /* 释放当前作用域的局部变量 */
    while (compiler->local_count > 0 &&
           compiler->locals[compiler->local_count - 1].depth > compiler->scope_depth) {
        
        Local *local = &compiler->locals[compiler->local_count - 1];
        
        /* 如果变量被捕获，关闭upvalue */
        if (local->is_captured) {
            xr_emit_ABC(compiler, OP_CLOSE, local->reg, 0, 0);
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
void xr_define_local(Compiler *compiler, XrString *name) {
    if (compiler->local_count >= MAXREGS) {
        xr_compiler_error(compiler, "Too many local variables");
        return;
    }
    
    /* 分配寄存器 */
    int reg = xr_allocreg(compiler);
    
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
static void define_local_with_reg(Compiler *compiler, XrString *name, int reg) {
    if (compiler->local_count >= MAXREGS) {
        xr_compiler_error(compiler, "Too many local variables");
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
static int add_upvalue(Compiler *compiler, uint8_t index, bool is_local) {
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
        xr_compiler_error(compiler, "Too many upvalues");
        return 0;
    }
    
    compiler->upvalues[upvalue_count].index = index;
    compiler->upvalues[upvalue_count].is_local = is_local;
    
    return xr_bc_proto_add_upvalue(compiler->proto, index, is_local);
}

/*
** 解析upvalue
*/
int xr_resolve_upvalue(Compiler *compiler, XrString *name) {
    if (compiler->enclosing == NULL) {
        return -1;  /* 顶层函数，没有upvalue */
    }
    
    /* 在外层编译器中查找局部变量 */
    int local = xr_resolve_local(compiler->enclosing, name);
    if (local != -1) {
        /* 标记为被捕获 */
        compiler->enclosing->locals[local].is_captured = true;
        return add_upvalue(compiler, (uint8_t)local, true);
    }
    
    /* 在外层upvalue中查找 */
    int upvalue = xr_resolve_upvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return add_upvalue(compiler, (uint8_t)upvalue, false);
    }
    
    return -1;  /* 未找到 */
}

/* ========== 编译器初始化 ========== */

/*
** 初始化编译器
*/
void xr_compiler_init(Compiler *compiler, FunctionType type) {
    compiler->enclosing = current;
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
    current = compiler;
}

/*
** 结束编译
*/
Proto *xr_compiler_end(Compiler *compiler) {
    /* 发射返回指令 */
    xr_emit_ABC(compiler, OP_RETURN, 0, 0, 0);
    
    Proto *proto = compiler->proto;
    
    /* 恢复外层编译器 */
    current = compiler->enclosing;
    
    /* 如果有错误，释放proto */
    if (compiler->had_error) {
        xr_bc_proto_free(proto);
        return NULL;
    }
    
    return proto;
}

/* ========== 表达式编译 ========== */

/*
** 编译字面量表达式
*/
static int compile_literal(Compiler *compiler, LiteralNode *node) {
    int reg = xr_allocreg(compiler);
    
    XrType type = xr_value_type(node->value);
    switch (type) {
        case XR_TNULL:
            xr_emit_ABC(compiler, OP_LOADNIL, reg, 0, 0);
            break;
        
        case XR_TBOOL:
            if (xr_tobool(node->value)) {
                xr_emit_ABC(compiler, OP_LOADTRUE, reg, 0, 0);
            } else {
                xr_emit_ABC(compiler, OP_LOADFALSE, reg, 0, 0);
            }
            break;
        
        case XR_TINT: {
            xr_Integer ival = xr_toint(node->value);
            /* 尝试使用立即数指令 */
            if (ival >= -MAXARG_sBx && ival <= MAXARG_sBx) {
                xr_emit_AsBx(compiler, OP_LOADI, reg, (int)ival);
            } else {
                /* 使用常量池 */
                int kidx = xr_bc_proto_add_constant(compiler->proto, node->value);
                xr_emit_ABx(compiler, OP_LOADK, reg, kidx);
            }
            break;
        }
        
        case XR_TFLOAT: {
            /* 浮点数总是使用常量池 */
            int kidx = xr_bc_proto_add_constant(compiler->proto, node->value);
            xr_emit_ABx(compiler, OP_LOADK, reg, kidx);
            break;
        }
        
        case XR_TSTRING: {
            int kidx = xr_bc_proto_add_constant(compiler->proto, node->value);
            xr_emit_ABx(compiler, OP_LOADK, reg, kidx);
            break;
        }
        
        default:
            xr_compiler_error(compiler, "Unsupported literal type");
            break;
    }
    
    return reg;
}

/*
** 编译逻辑与运算（短路求值）
*/
static int compile_and(Compiler *compiler, BinaryNode *node) {
    /* 编译左操作数 */
    int rb = xr_compile_expression(compiler, node->left);
    
    /* if not left then skip right */
    xr_emit_ABC(compiler, OP_TESTSET, rb, rb, 1);  /* 如果false则跳过 */
    int jump = xr_emit_jump(compiler, OP_JMP);
    
    /* 编译右操作数 */
    int rc = xr_compile_expression(compiler, node->right);
    
    /* 移动结果 */
    xr_emit_ABC(compiler, OP_MOVE, rb, rc, 0);
    xr_freereg(compiler, rc);
    
    /* 回填跳转 */
    xr_patch_jump(compiler, jump);
    
    return rb;
}

/*
** 编译逻辑或运算（短路求值）
*/
static int compile_or(Compiler *compiler, BinaryNode *node) {
    /* 编译左操作数 */
    int rb = xr_compile_expression(compiler, node->left);
    
    /* if left then skip right */
    xr_emit_ABC(compiler, OP_TESTSET, rb, rb, 0);  /* 如果true则跳过 */
    int jump = xr_emit_jump(compiler, OP_JMP);
    
    /* 编译右操作数 */
    int rc = xr_compile_expression(compiler, node->right);
    
    /* 移动结果 */
    xr_emit_ABC(compiler, OP_MOVE, rb, rc, 0);
    xr_freereg(compiler, rc);
    
    /* 回填跳转 */
    xr_patch_jump(compiler, jump);
    
    return rb;
}

/*
** 编译二元运算表达式
*/
static int compile_binary(Compiler *compiler, BinaryNode *node, AstNodeType type) {
    /* 逻辑运算需要短路求值 */
    if (type == AST_BINARY_AND) {
        return compile_and(compiler, node);
    }
    if (type == AST_BINARY_OR) {
        return compile_or(compiler, node);
    }
    
    /* 优化：检查右操作数是否是小整数常量 */
    if (node->right->type == AST_LITERAL_INT) {
        LiteralNode *lit = (LiteralNode *)&node->right->as;
        xr_Integer value = xr_toint(lit->value);
        
        /* 如果是小整数（-128到127），使用立即数指令 */
        if (value >= -128 && value <= 127) {
            int rb = xr_compile_expression(compiler, node->left);
            int ra = xr_allocreg(compiler);
            
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
                xr_emit_ABsC(compiler, op, ra, rb, (int)value);
                xr_freereg(compiler, rb);
                return ra;
            }
        }
    }
    
    /* 通用路径：编译两个操作数 */
    int rb = xr_compile_expression(compiler, node->left);
    int rc = xr_compile_expression(compiler, node->right);
    int ra = xr_allocreg(compiler);
    
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
            xr_compiler_error(compiler, "Unknown binary operator: %d", type);
            return ra;
    }
    
    /* 发射指令 */
    xr_emit_ABC(compiler, op, ra, rb, rc);
    
    /* 释放操作数寄存器 */
    xr_freereg(compiler, rb);
    xr_freereg(compiler, rc);
    
    return ra;
}

/*
** 编译比较运算（返回布尔值）
*/
static int compile_comparison(Compiler *compiler, BinaryNode *node, AstNodeType type) {
    /* 编译左操作数 */
    int rb = xr_compile_expression(compiler, node->left);
    
    /* 编译右操作数 */
    int rc = xr_compile_expression(compiler, node->right);
    
    /* 分配目标寄存器 */
    int ra = xr_allocreg(compiler);
    
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
            xr_compiler_error(compiler, "Unknown comparison operator: %d", type);
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
    xr_emit_ABC(compiler, op, rb, rc, 1);
    
    /* 比较为true时的跳转 */
    int true_jump = xr_emit_jump(compiler, OP_JMP);
    
    /* 比较为false：加载false（跳过了JMP才到这里） */
    if (negate) {
        xr_emit_ABC(compiler, OP_LOADTRUE, ra, 0, 0);
    } else {
        xr_emit_ABC(compiler, OP_LOADFALSE, ra, 0, 0);
    }
    
    /* 跳过true分支 */
    int end_jump = xr_emit_jump(compiler, OP_JMP);
    
    /* 回填true跳转 */
    xr_patch_jump(compiler, true_jump);
    
    /* 比较为true：加载true */
    if (negate) {
        xr_emit_ABC(compiler, OP_LOADFALSE, ra, 0, 0);
    } else {
        xr_emit_ABC(compiler, OP_LOADTRUE, ra, 0, 0);
    }
    
    /* 回填end跳转 */
    xr_patch_jump(compiler, end_jump);
    
    /* 释放操作数寄存器 */
    xr_freereg(compiler, rb);
    xr_freereg(compiler, rc);
    
    return ra;
}

/*
** 编译一元运算表达式
*/
static int compile_unary(Compiler *compiler, UnaryNode *node, AstNodeType type) {
    /* 编译操作数 */
    int rb = xr_compile_expression(compiler, node->operand);
    
    /* 分配目标寄存器 */
    int ra = xr_allocreg(compiler);
    
    /* 根据节点类型选择指令 */
    OpCode op;
    switch (type) {
        case AST_UNARY_NEG: op = OP_UNM; break;
        case AST_UNARY_NOT: op = OP_NOT; break;
        default:
            xr_compiler_error(compiler, "Unknown unary operator: %d", type);
            return ra;
    }
    
    /* 发射指令 */
    xr_emit_ABC(compiler, op, ra, rb, 0);
    
    /* 释放操作数寄存器 */
    xr_freereg(compiler, rb);
    
    return ra;
}

/*
** 编译变量访问表达式
*/
static int compile_variable(Compiler *compiler, VariableNode *node) {
    /* 创建临时字符串用于查找 */
    XrString *name_str = xr_string_new(node->name, strlen(node->name));
    
    /* 查找局部变量 */
    int reg = xr_resolve_local(compiler, name_str);
    if (reg >= 0) {
        return reg;  /* 局部变量，直接返回其寄存器 */
    }
    
    /* 查找upvalue */
    int upvalue = xr_resolve_upvalue(compiler, name_str);
    if (upvalue >= 0) {
        int ra = xr_allocreg(compiler);
        xr_emit_ABC(compiler, OP_GETUPVAL, ra, upvalue, 0);
        return ra;
    }
    
    /* 全局变量（Wren风格：使用固定索引） */
    int ra = xr_allocreg(compiler);
    int global_index = get_or_add_global(compiler, name_str);
    xr_emit_ABx(compiler, OP_GETGLOBAL, ra, global_index);  /* 索引而非常量 */
    return ra;
}

/*
** 编译表达式（主入口）
*/
int xr_compile_expression(Compiler *compiler, AstNode *node) {
    if (node == NULL) {
        xr_compiler_error(compiler, "NULL expression node");
        return xr_allocreg(compiler);
    }
    
    switch (node->type) {
        /* 字面量 */
        case AST_LITERAL_INT:
        case AST_LITERAL_FLOAT:
        case AST_LITERAL_STRING:
        case AST_LITERAL_NULL:
        case AST_LITERAL_TRUE:
        case AST_LITERAL_FALSE:
            return compile_literal(compiler, (LiteralNode *)&node->as);
        
        /* 二元运算 */
        /* 算术运算 */
        case AST_BINARY_ADD:
        case AST_BINARY_SUB:
        case AST_BINARY_MUL:
        case AST_BINARY_DIV:
        case AST_BINARY_MOD:
            return compile_binary(compiler, (BinaryNode *)&node->as, node->type);
        
        /* 比较运算（返回布尔值） */
        case AST_BINARY_EQ:
        case AST_BINARY_NE:
        case AST_BINARY_LT:
        case AST_BINARY_LE:
        case AST_BINARY_GT:
        case AST_BINARY_GE:
            return compile_comparison(compiler, (BinaryNode *)&node->as, node->type);
        
        /* 逻辑运算（短路求值） */
        case AST_BINARY_AND:
        case AST_BINARY_OR:
            return compile_binary(compiler, (BinaryNode *)&node->as, node->type);
        
        /* 一元运算 */
        case AST_UNARY_NEG:
        case AST_UNARY_NOT:
            return compile_unary(compiler, (UnaryNode *)&node->as, node->type);
        
        /* 变量引用 */
        case AST_VARIABLE:
            return compile_variable(compiler, (VariableNode *)&node->as);
        
        /* 分组表达式 */
        case AST_GROUPING:
            return xr_compile_expression(compiler, node->as.grouping);
        
        /* 赋值表达式（作为表达式使用时） */
        case AST_ASSIGNMENT: {
            compile_assignment(compiler, &node->as.assignment);
            /* TODO: 返回赋值的值 */
            return xr_allocreg(compiler);
        }
        
        /* 函数调用 */
        case AST_CALL_EXPR:
            return compile_call(compiler, (CallExprNode *)&node->as);
        
        /* 数组操作 */
        case AST_ARRAY_LITERAL:
            return compile_array_literal(compiler, &node->as.array_literal);
        
        case AST_INDEX_GET:
            return compile_index_get(compiler, &node->as.index_get);
        
        default:
            xr_compiler_error(compiler, "Unsupported expression type: %d (%s at line %d)", 
                            node->type, 
                            (node->type == 41 ? "AST_BINARY_EQ?" : "Unknown"),
                            node->line);
            return xr_allocreg(compiler);
    }
}

/* ========== 语句编译 ========== */

/*
** 编译表达式语句
*/
static void compile_expr_stmt(Compiler *compiler, AstNode *expr) {
    /* 特殊处理赋值语句（它们不是表达式） */
    if (expr->type == AST_ASSIGNMENT) {
        compile_assignment(compiler, &expr->as.assignment);
        return;
    }
    if (expr->type == AST_INDEX_SET) {
        compile_index_set(compiler, &expr->as.index_set);
        return;
    }
    
    /* 其他表达式语句 */
    int reg = xr_compile_expression(compiler, expr);
    xr_freereg(compiler, reg);
}

/*
** 编译变量声明
*/
static void compile_var_decl(Compiler *compiler, VarDeclNode *node) {
    /* 创建字符串 */
    XrString *name_str = xr_string_new(node->name, strlen(node->name));
    
    if (compiler->scope_depth == 0) {
        /* 全局变量（Wren风格：使用固定索引） */
        int reg = xr_compile_expression(compiler, node->initializer);
        
        int global_index = get_or_add_global(compiler, name_str);
        xr_emit_ABx(compiler, OP_SETGLOBAL, reg, global_index);  /* 索引而非常量 */
        
        xr_freereg(compiler, reg);
    } else {
        /* 局部变量 */
        /* 先定义变量（分配寄存器） */
        xr_define_local(compiler, name_str);
        
        /* 编译初始化表达式到该寄存器 */
        int local_reg = compiler->locals[compiler->local_count - 1].reg;
        int expr_reg = xr_compile_expression(compiler, node->initializer);
        
        /* 如果表达式结果不在目标寄存器，移动它 */
        if (expr_reg != local_reg) {
            xr_emit_ABC(compiler, OP_MOVE, local_reg, expr_reg, 0);
            xr_freereg(compiler, expr_reg);
        }
    }
}

/*
** 编译print语句
*/
static void compile_print(Compiler *compiler, PrintNode *node) {
    /* 编译表达式 */
    int reg = xr_compile_expression(compiler, node->expr);
    
    /* 发射打印指令 */
    xr_emit_ABC(compiler, OP_PRINT, reg, 0, 0);
    
    xr_freereg(compiler, reg);
}

/*
** 编译赋值语句
*/
static void compile_assignment(Compiler *compiler, AssignmentNode *node) {
    /* 创建字符串 */
    XrString *name_str = xr_string_new(node->name, strlen(node->name));
    
    /* 编译右值 */
    int value_reg = xr_compile_expression(compiler, node->value);
    
    /* 查找变量 */
    int local = xr_resolve_local(compiler, name_str);
    if (local >= 0) {
        /* 局部变量赋值 */
        if (value_reg != local) {
            xr_emit_ABC(compiler, OP_MOVE, local, value_reg, 0);
            xr_freereg(compiler, value_reg);
        }
    } else {
        int upvalue = xr_resolve_upvalue(compiler, name_str);
        if (upvalue >= 0) {
            /* Upvalue赋值 */
            xr_emit_ABC(compiler, OP_SETUPVAL, value_reg, upvalue, 0);
            xr_freereg(compiler, value_reg);
        } else {
            /* 全局变量赋值（Wren风格：使用固定索引） */
            int global_index = get_or_add_global(compiler, name_str);
            xr_emit_ABx(compiler, OP_SETGLOBAL, value_reg, global_index);  /* 索引而非常量 */
            xr_freereg(compiler, value_reg);
        }
    }
}

/*
** 编译if语句
*/
static void compile_if(Compiler *compiler, IfStmtNode *node) {
    /* 编译条件 */
    int cond_reg = xr_compile_expression(compiler, node->condition);
    
    /* if cond is false then jump to else
     * OP_TEST的语义：if is_falsey(R[a]) == k then skip next
     * k=1: 当值为false时跳过（即条件为false时跳到else）
     * k=0: 当值为true时跳过（即条件为true时跳到then）
     * 
     * 我们想要：当条件为false时跳到else
     * 所以检查：if is_falsey(R[a]) == 1 (即R[a]是false)，则跳过JMP
     * 
     * 等等，这样还是不对！让我重新思考...
     * 
     * 实际上：
     * TEST R[a] 1: 如果R[a]为false，跳过下一条JMP -> 不跳转 -> 执行then
     * 这是反的！
     * 
     * TEST R[a] 0: 如果R[a]为true，跳过下一条JMP -> 不跳转 -> 执行then
     * 这才对！
     */
    xr_emit_ABC(compiler, OP_TEST, cond_reg, 0, 0);  /* k=0表示true时跳过JMP */
    int then_jump = xr_emit_jump(compiler, OP_JMP);
    xr_freereg(compiler, cond_reg);
    
    /* 编译then分支 */
    xr_compile_statement(compiler, node->then_branch);
    
    /* 跳过else分支 */
    int else_jump = xr_emit_jump(compiler, OP_JMP);
    
    /* 回填then_jump */
    xr_patch_jump(compiler, then_jump);
    
    /* 编译else分支（如果有） */
    if (node->else_branch != NULL) {
        xr_compile_statement(compiler, node->else_branch);
    }
    
    /* 回填else_jump */
    xr_patch_jump(compiler, else_jump);
}

/*
** 编译while循环
*/
static void compile_while(Compiler *compiler, WhileStmtNode *node) {
    int loop_start = compiler->proto->sizecode;
    
    /* 编译条件 */
    int cond_reg = xr_compile_expression(compiler, node->condition);
    
    /* if not cond then exit loop */
    xr_emit_ABC(compiler, OP_TEST, cond_reg, 1, 0);
    int exit_jump = xr_emit_jump(compiler, OP_JMP);
    xr_freereg(compiler, cond_reg);
    
    /* 编译循环体 */
    compiler->loop_depth++;
    compiler->loop_start = loop_start;
    xr_compile_statement(compiler, node->body);
    compiler->loop_depth--;
    
    /* 跳回循环开始 */
    xr_emit_loop(compiler, loop_start);
    
    /* 回填退出跳转 */
    xr_patch_jump(compiler, exit_jump);
}

/*
** 编译for循环
*/
static void compile_for(Compiler *compiler, ForStmtNode *node) {
    /* 进入循环作用域 */
    xr_begin_scope(compiler);
    
    /* 编译初始化 */
    if (node->initializer != NULL) {
        xr_compile_statement(compiler, node->initializer);
    }
    
    int loop_start = compiler->proto->sizecode;
    
    /* 编译条件 */
    int exit_jump = -1;
    if (node->condition != NULL) {
        int cond_reg = xr_compile_expression(compiler, node->condition);
        /* OP_TEST k=0: 如果条件为false则跳过下一条指令（跳过JMP，继续循环）*/
        /* 我们希望false时跳出循环，所以需要k=0，然后JMP跳出 */
        /* 但实际逻辑是：TEST如果false跳过JMP，我们想false时跳出，所以不应该跳过JMP */
        /* 正确的逻辑：k=0表示false时跳过，我们要false时跳出，所以k应该是0，然后反转JMP逻辑 */
        /* 简化：TEST k=0 + JMP表示"false时跳过JMP继续，true时执行JMP跳出" */
        /* 这正好相反！我们要"false时跳出，true时继续" */
        /* 所以应该用k=0，但是不跳过JMP，而是跳过继续的代码 */
        /* 让我换个思路：if (cond) { body } 应该是 TEST cond 0; JMP exit; body; exit: */
        xr_emit_ABC(compiler, OP_TEST, cond_reg, 0, 0);
        exit_jump = xr_emit_jump(compiler, OP_JMP);
        xr_freereg(compiler, cond_reg);
    }
    
    /* 编译循环体 */
    compiler->loop_depth++;
    compiler->loop_start = loop_start;
    xr_compile_statement(compiler, node->body);
    compiler->loop_depth--;
    
    /* 编译更新表达式 */
    if (node->increment != NULL) {
        int inc_reg = xr_compile_expression(compiler, node->increment);
        xr_freereg(compiler, inc_reg);
    }
    
    /* 跳回循环开始 */
    xr_emit_loop(compiler, loop_start);
    
    /* 回填退出跳转 */
    if (exit_jump != -1) {
        xr_patch_jump(compiler, exit_jump);
    }
    
    /* 退出循环作用域 */
    xr_end_scope(compiler);
}

/*
** 编译函数定义
*/
static void compile_function(Compiler *compiler, FunctionDeclNode *node) {
    /* 如果是命名局部函数，先在当前作用域定义函数名 */
    /* 这样函数体编译时可以通过upvalue递归访问自己 */
    int func_reg = -1;
    XrString *name_str = NULL;
    
    if (node->name != NULL && compiler->scope_depth > 0) {
        /* 局部函数：先分配寄存器并定义变量（值暂时为nil） */
        name_str = xr_string_new(node->name, strlen(node->name));
        func_reg = xr_allocreg(compiler);
        define_local_with_reg(compiler, name_str, func_reg);
    }
    
    /* 创建新的编译器（嵌套） */
    Compiler function_compiler;
    xr_compiler_init(&function_compiler, FUNCTION_FUNCTION);
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
        xr_define_local(&function_compiler, param_str);
    }
    
    /* 编译函数体 */
    xr_compile_statement(&function_compiler, node->body);
    
    /* 结束编译 */
    Proto *proto = xr_compiler_end(&function_compiler);
    
    if (proto != NULL) {
        /* 将函数原型添加到父编译器 */
        int proto_idx = xr_bc_proto_add_proto(compiler->proto, proto);
        
        /* 如果是命名函数，定义为变量 */
        if (node->name != NULL) {
            if (compiler->scope_depth == 0) {
                /* 全局函数（Wren风格：使用固定索引） */
                int reg = xr_allocreg(compiler);
                xr_emit_ABx(compiler, OP_CLOSURE, reg, proto_idx);
                
                int global_index = get_or_add_global(compiler, name_str);
                xr_emit_ABx(compiler, OP_SETGLOBAL, reg, global_index);  /* 索引而非常量 */
                xr_freereg(compiler, reg);
            } else {
                /* 局部函数 - 使用之前分配的寄存器，现在填充闭包 */
                xr_emit_ABx(compiler, OP_CLOSURE, func_reg, proto_idx);
                /* 函数名已经在前面定义，不需要再定义 */
            }
        } else {
            /* 匿名函数表达式 */
            int reg = xr_allocreg(compiler);
            xr_emit_ABx(compiler, OP_CLOSURE, reg, proto_idx);
            /* 不定义变量，返回寄存器 */
        }
    }
}

/*
** 编译函数调用
** is_tail: 是否是尾调用位置（Phase 2新增）
*/
static int compile_call_internal(Compiler *compiler, CallExprNode *node, bool is_tail) {
    /* 编译被调用的函数表达式 */
    int func_reg = xr_compile_expression(compiler, node->callee);
    
    /* 编译参数 */
    for (int i = 0; i < node->arg_count; i++) {
        int arg_reg = xr_compile_expression(compiler, node->arguments[i]);
        /* 参数应该在连续的寄存器中 */
        if (arg_reg != func_reg + i + 1) {
            xr_emit_ABC(compiler, OP_MOVE, func_reg + i + 1, arg_reg, 0);
            xr_freereg(compiler, arg_reg);
        }
    }
    
    /* ⭐ Phase 2: 根据位置选择CALL或TAILCALL */
    if (is_tail && compiler->type == FUNCTION_FUNCTION) {
        /* 尾调用：复用栈帧，不增加调用深度 */
        xr_emit_ABC(compiler, OP_TAILCALL, func_reg, node->arg_count, 0);
        /* 尾调用后不返回值到寄存器（直接返回） */
        return -1;  /* 特殊标记：无返回寄存器 */
    } else {
        /* 普通调用 */
        xr_emit_ABC(compiler, OP_CALL, func_reg, node->arg_count, 1);
        /* 结果在func_reg中 */
        return func_reg;
    }
}

/*
** 编译函数调用（普通版本，兼容性包装）
*/
static int compile_call(Compiler *compiler, CallExprNode *node) {
    return compile_call_internal(compiler, node, false);
}

/*
** 编译return语句
** ⭐ Phase 2: 支持尾调用优化
*/
static void compile_return(Compiler *compiler, ReturnStmtNode *node) {
    if (compiler->type == FUNCTION_SCRIPT) {
        xr_compiler_error(compiler, "Cannot return from top-level code");
        return;
    }
    
    if (node->value != NULL) {
        /* ⭐ Phase 2: 检测尾调用
        ** 如果return的表达式是函数调用，这就是尾调用位置
        */
        if (node->value->type == AST_CALL_EXPR) {
            /* 尾调用优化：使用OP_TAILCALL代替OP_CALL+OP_RETURN */
            compile_call_internal(compiler, (CallExprNode *)&node->value->as, true);
            /* TAILCALL指令已经包含了return语义，无需额外的RETURN指令 */
        } else {
            /* 普通return：先计算表达式，再返回 */
            int reg = xr_compile_expression(compiler, node->value);
            xr_emit_ABC(compiler, OP_RETURN, reg, 1, 0);
            xr_freereg(compiler, reg);
        }
    } else {
        /* 返回null */
        xr_emit_ABC(compiler, OP_RETURN, 0, 0, 0);
    }
}

/*
** 编译数组字面量
*/
static int compile_array_literal(Compiler *compiler, ArrayLiteralNode *node) {
    /* 分配目标寄存器 */
    int array_reg = xr_allocreg(compiler);
    
    /* 创建空数组 */
    /* NEWTABLE A B C: R[A] = {} (B=数组大小提示, C=哈希大小提示) */
    xr_emit_ABC(compiler, OP_NEWTABLE, array_reg, node->count, 0);
    
    /* 使用SETLIST批量设置元素 */
    if (node->count > 0) {
        /* 编译所有元素到连续寄存器 */
        for (int i = 0; i < node->count; i++) {
            int elem_reg = xr_compile_expression(compiler, node->elements[i]);
            
            /* 确保元素在连续寄存器中 */
            int target_reg = array_reg + i + 1;
            if (elem_reg != target_reg) {
                xr_emit_ABC(compiler, OP_MOVE, target_reg, elem_reg, 0);
                xr_freereg(compiler, elem_reg);
            }
        }
        
        /* SETLIST A B C: R[A][i] = R[A+i], 1 <= i <= B */
        xr_emit_ABC(compiler, OP_SETLIST, array_reg, node->count, 0);
        
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
static int compile_index_get(Compiler *compiler, IndexGetNode *node) {
    /* 编译数组表达式 */
    int array_reg = xr_compile_expression(compiler, node->array);
    
    /* 编译索引表达式 */
    int index_reg = xr_compile_expression(compiler, node->index);
    
    /* 分配结果寄存器 */
    int result_reg = xr_allocreg(compiler);
    
    /* 使用通用GETTABLE指令 */
    /* TODO: 添加GETI优化（检测整数常量索引） */
    xr_emit_ABC(compiler, OP_GETTABLE, result_reg, array_reg, index_reg);
    xr_freereg(compiler, index_reg);
    
    xr_freereg(compiler, array_reg);
    
    return result_reg;
}

/*
** 编译索引赋值
*/
static void compile_index_set(Compiler *compiler, IndexSetNode *node) {
    /* 编译数组表达式 */
    int array_reg = xr_compile_expression(compiler, node->array);
    
    /* 编译索引表达式 */
    int index_reg = xr_compile_expression(compiler, node->index);
    
    /* 编译值表达式 */
    int value_reg = xr_compile_expression(compiler, node->value);
    
    /* 使用通用SETTABLE指令 */
    /* TODO: 添加SETI优化（检测整数常量索引） */
    xr_emit_ABC(compiler, OP_SETTABLE, array_reg, index_reg, value_reg);
    xr_freereg(compiler, index_reg);
    
    xr_freereg(compiler, value_reg);
    xr_freereg(compiler, array_reg);
}

/*
** 编译语句（主入口）
*/
void xr_compile_statement(Compiler *compiler, AstNode *node) {
    if (node == NULL) {
        return;
    }
    
    /* 更新当前行号 */
    current_line = node->line;
    
    switch (node->type) {
        case AST_EXPR_STMT:
            compile_expr_stmt(compiler, node->as.expr_stmt);
            break;
        
        case AST_PRINT_STMT:
            compile_print(compiler, &node->as.print_stmt);
            break;
        
        case AST_VAR_DECL:
        case AST_CONST_DECL:
            compile_var_decl(compiler, &node->as.var_decl);
            break;
        
        case AST_ASSIGNMENT:
            compile_assignment(compiler, &node->as.assignment);
            break;
        
        case AST_IF_STMT:
            compile_if(compiler, &node->as.if_stmt);
            break;
        
        case AST_WHILE_STMT:
            compile_while(compiler, &node->as.while_stmt);
            break;
        
        case AST_FOR_STMT:
            compile_for(compiler, &node->as.for_stmt);
            break;
        
        case AST_FUNCTION_DECL:
            compile_function(compiler, &node->as.function_decl);
            break;
        
        case AST_RETURN_STMT:
            compile_return(compiler, &node->as.return_stmt);
            break;
        
        case AST_INDEX_SET:
            compile_index_set(compiler, &node->as.index_set);
            break;
        
        case AST_BLOCK: {
            BlockNode *block = &node->as.block;
            xr_begin_scope(compiler);
            for (int i = 0; i < block->count; i++) {
                xr_compile_statement(compiler, block->statements[i]);
            }
            xr_end_scope(compiler);
            break;
        }
        
        case AST_PROGRAM: {
            ProgramNode *program = &node->as.program;
            for (int i = 0; i < program->count; i++) {
                xr_compile_statement(compiler, program->statements[i]);
            }
            break;
        }
        
        default:
            xr_compiler_error(compiler, "Unsupported statement type: %d", node->type);
            break;
    }
}

/* ========== 编译API ========== */

/* ========== 全局变量索引管理（Wren风格优化）========== */

/*
** 获取或添加全局变量索引
*/
static int get_or_add_global(Compiler *compiler, XrString *name) {
    /* 查找是否已存在 */
    for (int i = 0; i < global_var_count; i++) {
        if (global_vars[i].name != NULL && 
            strcmp(global_vars[i].name->chars, name->chars) == 0) {
            return i;
        }
    }
    
    /* 添加新的全局变量 */
    if (global_var_count >= MAX_GLOBALS) {
        xr_compiler_error(compiler, "Too many global variables (max %d)", MAX_GLOBALS);
        return 0;
    }
    
    int index = global_var_count++;
    global_vars[index].name = name;
    global_vars[index].index = index;
    
    return index;
}

/*
** 编译AST到函数原型
*/
Proto *xr_compile(AstNode *ast) {
    /* 重置全局变量计数（每次编译重新开始） */
    global_var_count = 0;
    for (int i = 0; i < MAX_GLOBALS; i++) {
        global_vars[i].name = NULL;
        global_vars[i].index = -1;
    }
    
    Compiler compiler;
    xr_compiler_init(&compiler, FUNCTION_SCRIPT);
    
    /* 设置全局变量数组指针 */
    compiler.globals = global_vars;
    compiler.global_count = &global_var_count;
    
    /* 编译AST */
    xr_compile_statement(&compiler, ast);
    
    /* 将全局变量数量保存到Proto（用于VM初始化） */
    compiler.proto->num_globals = global_var_count;
    
    /* 结束编译 */
    return xr_compiler_end(&compiler);
}


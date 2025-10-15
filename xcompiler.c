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

/* ========== 前向声明 ========== */

static int compile_literal(Compiler *compiler, LiteralNode *node);
static int compile_binary(Compiler *compiler, BinaryNode *node, AstNodeType type);
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
    
    switch (node->value.type) {
        case XR_TNULL:
            xr_emit_ABC(compiler, OP_LOADNIL, reg, 0, 0);
            break;
        
        case XR_TBOOL:
            if (node->value.as.b) {
                xr_emit_ABC(compiler, OP_LOADTRUE, reg, 0, 0);
            } else {
                xr_emit_ABC(compiler, OP_LOADFALSE, reg, 0, 0);
            }
            break;
        
        case XR_TINT: {
            xr_Integer ival = node->value.as.i;
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
    
    /* 编译左操作数 */
    int rb = xr_compile_expression(compiler, node->left);
    
    /* 编译右操作数 */
    int rc = xr_compile_expression(compiler, node->right);
    
    /* 分配目标寄存器 */
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
        
        /* 比较运算（暂时使用基础指令，后续优化） */
        case AST_BINARY_EQ: op = OP_EQ; break;
        case AST_BINARY_NE: op = OP_EQ; break;  /* 需要取反 */
        case AST_BINARY_LT: op = OP_LT; break;
        case AST_BINARY_LE: op = OP_LE; break;
        case AST_BINARY_GT: op = OP_LT; break;  /* 交换操作数 */
        case AST_BINARY_GE: op = OP_LE; break;  /* 交换操作数 */
        
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
    
    /* 全局变量 */
    int ra = xr_allocreg(compiler);
    XrValue name_val = xr_string_value(name_str);
    int kidx = xr_bc_proto_add_constant(compiler->proto, name_val);
    xr_emit_ABx(compiler, OP_GETGLOBAL, ra, kidx);
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
        case AST_BINARY_ADD:
        case AST_BINARY_SUB:
        case AST_BINARY_MUL:
        case AST_BINARY_DIV:
        case AST_BINARY_MOD:
        case AST_BINARY_EQ:
        case AST_BINARY_NE:
        case AST_BINARY_LT:
        case AST_BINARY_LE:
        case AST_BINARY_GT:
        case AST_BINARY_GE:
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
        
        default:
            xr_compiler_error(compiler, "Unsupported expression type: %d", node->type);
            return xr_allocreg(compiler);
    }
}

/* ========== 语句编译 ========== */

/*
** 编译表达式语句
*/
static void compile_expr_stmt(Compiler *compiler, AstNode *expr) {
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
        /* 全局变量 */
        int reg = xr_compile_expression(compiler, node->initializer);
        
        XrValue name_val = xr_string_value(name_str);
        int kidx = xr_bc_proto_add_constant(compiler->proto, name_val);
        xr_emit_ABx(compiler, OP_SETGLOBAL, reg, kidx);
        
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
    
    /* TODO: 调用内置print函数 */
    /* 暂时：直接发射打印指令（简化） */
    /* 将来：通过函数调用机制实现 */
    
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
            /* 全局变量赋值 */
            XrValue name_val = xr_string_value(name_str);
            int kidx = xr_bc_proto_add_constant(compiler->proto, name_val);
            xr_emit_ABx(compiler, OP_SETGLOBAL, value_reg, kidx);
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
    
    /* if not cond then jump to else */
    xr_emit_ABC(compiler, OP_TEST, cond_reg, 1, 0);  /* k=1表示false跳转 */
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
        xr_emit_ABC(compiler, OP_TEST, cond_reg, 1, 0);
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
    /* 创建新的编译器（嵌套） */
    Compiler function_compiler;
    xr_compiler_init(&function_compiler, FUNCTION_FUNCTION);
    function_compiler.enclosing = compiler;
    
    /* 设置函数名 */
    if (node->name != NULL) {
        XrString *name_str = xr_string_new(node->name, strlen(node->name));
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
        
        /* 创建闭包 */
        int reg = xr_allocreg(compiler);
        xr_emit_ABx(compiler, OP_CLOSURE, reg, proto_idx);
        
        /* 如果是命名函数，定义为变量 */
        if (node->name != NULL) {
            XrString *name_str = xr_string_new(node->name, strlen(node->name));
            
            if (compiler->scope_depth == 0) {
                /* 全局函数 */
                XrValue name_val = xr_string_value(name_str);
                int kidx = xr_bc_proto_add_constant(compiler->proto, name_val);
                xr_emit_ABx(compiler, OP_SETGLOBAL, reg, kidx);
                xr_freereg(compiler, reg);
            } else {
                /* 局部函数 */
                xr_define_local(compiler, name_str);
            }
        }
    }
}

/*
** 编译函数调用
*/
static int compile_call(Compiler *compiler, CallExprNode *node) {
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
    
    /* 发射调用指令 */
    /* CALL A B C: R[A]...R[A+C-2] = R[A](R[A+1]...R[A+B-1]) */
    xr_emit_ABC(compiler, OP_CALL, func_reg, node->arg_count, 1);
    
    /* 结果在func_reg中 */
    return func_reg;
}

/*
** 编译return语句
*/
static void compile_return(Compiler *compiler, ReturnStmtNode *node) {
    if (compiler->type == FUNCTION_SCRIPT) {
        xr_compiler_error(compiler, "Cannot return from top-level code");
        return;
    }
    
    if (node->value != NULL) {
        /* 编译返回值 */
        int reg = xr_compile_expression(compiler, node->value);
        xr_emit_ABC(compiler, OP_RETURN, reg, 1, 0);
        xr_freereg(compiler, reg);
    } else {
        /* 返回null */
        xr_emit_ABC(compiler, OP_RETURN, 0, 0, 0);
    }
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

/*
** 编译AST到函数原型
*/
Proto *xr_compile(AstNode *ast) {
    Compiler compiler;
    xr_compiler_init(&compiler, FUNCTION_SCRIPT);
    
    /* 编译AST */
    xr_compile_statement(&compiler, ast);
    
    /* 结束编译 */
    return xr_compiler_end(&compiler);
}


/*
** xast.c
** Xray AST 实现
** 实现抽象语法树节点的创建、操作和释放
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "xast.h"

/* 程序节点初始容量 */
#define INITIAL_CAPACITY 8

/*
** 分配 AST 节点内存
** 返回新分配的节点指针
*/
static AstNode *alloc_node(XrayState *X, AstNodeType type, int line) {
    AstNode *node = (AstNode *)malloc(sizeof(AstNode));
    if (node == NULL) {
        fprintf(stderr, "内存分配失败\n");
        exit(1);
    }
    node->type = type;
    node->line = line;
    return node;
}

/* ========== 字面量节点创建 ========== */

/*
** 创建整数字面量节点
*/
AstNode *xr_ast_literal_int(XrayState *X, xr_Integer value, int line) {
    AstNode *node = alloc_node(X, AST_LITERAL_INT, line);
    xr_setint(&node->as.literal.value, value);
    return node;
}

/*
** 创建浮点数字面量节点
*/
AstNode *xr_ast_literal_float(XrayState *X, xr_Number value, int line) {
    AstNode *node = alloc_node(X, AST_LITERAL_FLOAT, line);
    xr_setfloat(&node->as.literal.value, value);
    return node;
}

/*
** 创建字符串字面量节点
** 注意：暂时直接存储指针，后续会实现字符串对象
*/
AstNode *xr_ast_literal_string(XrayState *X, const char *value, int line) {
    AstNode *node = alloc_node(X, AST_LITERAL_STRING, line);
    /* 暂时简单实现：直接存储字符串指针 */
    /* TODO: 后续实现字符串对象和驻留 */
    char *str = (char *)malloc(strlen(value) + 1);
    strcpy(str, value);
    node->as.literal.value.type = XR_TSTRING;
    node->as.literal.value.as.obj = str;
    return node;
}

/*
** 创建 null 字面量节点
*/
AstNode *xr_ast_literal_null(XrayState *X, int line) {
    AstNode *node = alloc_node(X, AST_LITERAL_NULL, line);
    xr_setnull(&node->as.literal.value);
    return node;
}

/*
** 创建布尔字面量节点
** value: 0 表示 false，非 0 表示 true
*/
AstNode *xr_ast_literal_bool(XrayState *X, int value, int line) {
    AstNode *node = alloc_node(X, value ? AST_LITERAL_TRUE : AST_LITERAL_FALSE, line);
    xr_setbool(&node->as.literal.value, value ? 1 : 0);
    return node;
}

/* ========== 运算符节点创建 ========== */

/*
** 创建二元运算节点
** type: 运算符类型（加减乘除、比较、逻辑等）
** left: 左操作数
** right: 右操作数
*/
AstNode *xr_ast_binary(XrayState *X, AstNodeType type,
                       AstNode *left, AstNode *right, int line) {
    AstNode *node = alloc_node(X, type, line);
    node->as.binary.left = left;
    node->as.binary.right = right;
    return node;
}

/*
** 创建一元运算节点
** type: 运算符类型（取负、逻辑非）
** operand: 操作数
*/
AstNode *xr_ast_unary(XrayState *X, AstNodeType type,
                      AstNode *operand, int line) {
    AstNode *node = alloc_node(X, type, line);
    node->as.unary.operand = operand;
    return node;
}

/* ========== 其他节点创建 ========== */

/*
** 创建分组节点（括号表达式）
*/
AstNode *xr_ast_grouping(XrayState *X, AstNode *expr, int line) {
    AstNode *node = alloc_node(X, AST_GROUPING, line);
    node->as.grouping = expr;
    return node;
}

/*
** 创建表达式语句节点
*/
AstNode *xr_ast_expr_stmt(XrayState *X, AstNode *expr, int line) {
    AstNode *node = alloc_node(X, AST_EXPR_STMT, line);
    node->as.expr_stmt = expr;
    return node;
}

/*
** 创建 print 语句节点
*/
AstNode *xr_ast_print_stmt(XrayState *X, AstNode *expr, int line) {
    AstNode *node = alloc_node(X, AST_PRINT_STMT, line);
    node->as.print_stmt.expr = expr;
    return node;
}

/* ========== 程序节点操作 ========== */

/*
** 创建程序节点
** 程序节点包含多个语句
*/
AstNode *xr_ast_program(XrayState *X) {
    AstNode *node = alloc_node(X, AST_PROGRAM, 0);
    node->as.program.statements = NULL;
    node->as.program.count = 0;
    node->as.program.capacity = 0;
    return node;
}

/*
** 向程序节点添加语句
** 使用动态数组存储语句列表
*/
void xr_ast_program_add(XrayState *X, AstNode *program, AstNode *stmt) {
    /* 确保有足够空间 */
    if (program->as.program.count >= program->as.program.capacity) {
        int old_capacity = program->as.program.capacity;
        int new_capacity = old_capacity < INITIAL_CAPACITY ? 
                          INITIAL_CAPACITY : old_capacity * 2;
        
        program->as.program.statements = (AstNode **)realloc(
            program->as.program.statements,
            sizeof(AstNode *) * new_capacity
        );
        
        if (program->as.program.statements == NULL) {
            fprintf(stderr, "内存分配失败\n");
            exit(1);
        }
        
        program->as.program.capacity = new_capacity;
    }
    
    /* 添加语句 */
    program->as.program.statements[program->as.program.count++] = stmt;
}

/* ========== AST 释放 ========== */

/*
** 递归释放 AST 节点
** 释放节点及其所有子节点的内存
*/
void xr_ast_free(XrayState *X, AstNode *node) {
    if (node == NULL) return;
    
    switch (node->type) {
        /* 字面量节点 */
        case AST_LITERAL_INT:
        case AST_LITERAL_FLOAT:
        case AST_LITERAL_NULL:
        case AST_LITERAL_TRUE:
        case AST_LITERAL_FALSE:
            /* 简单值，直接释放 */
            break;
            
        case AST_LITERAL_STRING:
            /* 释放字符串内存 */
            if (node->as.literal.value.as.obj != NULL) {
                free(node->as.literal.value.as.obj);
            }
            break;
        
        /* 二元运算节点 */
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
            xr_ast_free(X, node->as.binary.left);
            xr_ast_free(X, node->as.binary.right);
            break;
        
        /* 一元运算节点 */
        case AST_UNARY_NEG:
        case AST_UNARY_NOT:
            xr_ast_free(X, node->as.unary.operand);
            break;
        
        /* 分组节点 */
        case AST_GROUPING:
            xr_ast_free(X, node->as.grouping);
            break;
        
        /* 表达式语句 */
        case AST_EXPR_STMT:
            xr_ast_free(X, node->as.expr_stmt);
            break;
        
        /* print 语句 */
        case AST_PRINT_STMT:
            xr_ast_free(X, node->as.print_stmt.expr);
            break;
        
        /* 程序节点 */
        case AST_PROGRAM:
            for (int i = 0; i < node->as.program.count; i++) {
                xr_ast_free(X, node->as.program.statements[i]);
            }
            if (node->as.program.statements != NULL) {
                free(node->as.program.statements);
            }
            break;
    }
    
    /* 释放节点本身 */
    free(node);
}

/* ========== 调试和工具函数 ========== */

/*
** 获取节点类型名称
** 用于调试和错误报告
*/
const char *xr_ast_typename(AstNodeType type) {
    switch (type) {
        case AST_LITERAL_INT:       return "LiteralInt";
        case AST_LITERAL_FLOAT:     return "LiteralFloat";
        case AST_LITERAL_STRING:    return "LiteralString";
        case AST_LITERAL_NULL:      return "LiteralNull";
        case AST_LITERAL_TRUE:      return "LiteralTrue";
        case AST_LITERAL_FALSE:     return "LiteralFalse";
        case AST_BINARY_ADD:        return "BinaryAdd";
        case AST_BINARY_SUB:        return "BinarySub";
        case AST_BINARY_MUL:        return "BinaryMul";
        case AST_BINARY_DIV:        return "BinaryDiv";
        case AST_BINARY_MOD:        return "BinaryMod";
        case AST_BINARY_EQ:         return "BinaryEq";
        case AST_BINARY_NE:         return "BinaryNe";
        case AST_BINARY_LT:         return "BinaryLt";
        case AST_BINARY_LE:         return "BinaryLe";
        case AST_BINARY_GT:         return "BinaryGt";
        case AST_BINARY_GE:         return "BinaryGe";
        case AST_BINARY_AND:        return "BinaryAnd";
        case AST_BINARY_OR:         return "BinaryOr";
        case AST_UNARY_NEG:         return "UnaryNeg";
        case AST_UNARY_NOT:         return "UnaryNot";
        case AST_GROUPING:          return "Grouping";
        case AST_EXPR_STMT:         return "ExprStmt";
        case AST_PRINT_STMT:        return "PrintStmt";
        case AST_PROGRAM:           return "Program";
        default:                    return "Unknown";
    }
}

/*
** 打印 AST 结构（用于调试）
** indent: 缩进级别
*/
void xr_ast_print(AstNode *node, int indent) {
    if (node == NULL) {
        printf("%*sNULL\n", indent * 2, "");
        return;
    }
    
    /* 打印缩进 */
    printf("%*s", indent * 2, "");
    
    /* 打印节点类型 */
    printf("%s", xr_ast_typename(node->type));
    
    /* 打印节点详细信息 */
    switch (node->type) {
        case AST_LITERAL_INT:
            printf("(%lld)", (long long)xr_toint(&node->as.literal.value));
            break;
        case AST_LITERAL_FLOAT:
            printf("(%g)", xr_tofloat(&node->as.literal.value));
            break;
        case AST_LITERAL_STRING:
            printf("(\"%s\")", (char *)node->as.literal.value.as.obj);
            break;
        case AST_LITERAL_NULL:
            printf("(null)");
            break;
        case AST_LITERAL_TRUE:
            printf("(true)");
            break;
        case AST_LITERAL_FALSE:
            printf("(false)");
            break;
        default:
            break;
    }
    
    printf("\n");
    
    /* 递归打印子节点 */
    switch (node->type) {
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
            xr_ast_print(node->as.binary.left, indent + 1);
            xr_ast_print(node->as.binary.right, indent + 1);
            break;
        
        case AST_UNARY_NEG:
        case AST_UNARY_NOT:
            xr_ast_print(node->as.unary.operand, indent + 1);
            break;
        
        case AST_GROUPING:
            xr_ast_print(node->as.grouping, indent + 1);
            break;
        
        case AST_EXPR_STMT:
            xr_ast_print(node->as.expr_stmt, indent + 1);
            break;
        
        case AST_PRINT_STMT:
            xr_ast_print(node->as.print_stmt.expr, indent + 1);
            break;
        
        case AST_PROGRAM:
            for (int i = 0; i < node->as.program.count; i++) {
                xr_ast_print(node->as.program.statements[i], indent + 1);
            }
            break;
        
        default:
            break;
    }
}


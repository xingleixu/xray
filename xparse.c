/*
** xparse.c
** Xray 语法解析器实现
** 使用 Pratt 解析器（上下文无关优先级解析）
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xparse.h"

/* ========== 解析规则表 ========== */

/*
** 解析规则表
** 为每种 Token 类型定义解析规则
** 这是 Pratt 解析器的核心数据结构
*/
static ParseRule rules[] = {
    /* Token 类型                前缀函数        中缀函数        优先级 */
    [TK_LPAREN]     = {xr_parse_grouping, NULL,           PREC_NONE},
    [TK_RPAREN]     = {NULL,              NULL,           PREC_NONE},
    [TK_LBRACE]     = {NULL,              NULL,           PREC_NONE},
    [TK_RBRACE]     = {NULL,              NULL,           PREC_NONE},
    [TK_LBRACKET]   = {NULL,              NULL,           PREC_NONE},
    [TK_RBRACKET]   = {NULL,              NULL,           PREC_NONE},
    [TK_COMMA]      = {NULL,              NULL,           PREC_NONE},
    [TK_DOT]        = {NULL,              NULL,           PREC_NONE},
    [TK_SEMICOLON]  = {NULL,              NULL,           PREC_NONE},
    
    /* 算术运算符 */
    [TK_PLUS]       = {NULL,              xr_parse_binary, PREC_TERM},
    [TK_MINUS]      = {xr_parse_unary,    xr_parse_binary, PREC_TERM},
    [TK_STAR]       = {NULL,              xr_parse_binary, PREC_FACTOR},
    [TK_SLASH]      = {NULL,              xr_parse_binary, PREC_FACTOR},
    [TK_PERCENT]    = {NULL,              xr_parse_binary, PREC_FACTOR},
    
    /* 比较运算符 */
    [TK_EQ]         = {NULL,              xr_parse_binary, PREC_EQUALITY},
    [TK_NE]         = {NULL,              xr_parse_binary, PREC_EQUALITY},
    [TK_LT]         = {NULL,              xr_parse_binary, PREC_COMPARISON},
    [TK_LE]         = {NULL,              xr_parse_binary, PREC_COMPARISON},
    [TK_GT]         = {NULL,              xr_parse_binary, PREC_COMPARISON},
    [TK_GE]         = {NULL,              xr_parse_binary, PREC_COMPARISON},
    
    /* 逻辑运算符 */
    [TK_AND]        = {NULL,              xr_parse_binary, PREC_AND},
    [TK_OR]         = {NULL,              xr_parse_binary, PREC_OR},
    [TK_NOT]        = {xr_parse_unary,    NULL,           PREC_NONE},
    
    /* 赋值 */
    [TK_ASSIGN]     = {NULL,              xr_parse_assignment, PREC_ASSIGNMENT},
    
    /* 关键字 */
    [TK_LET]        = {NULL,              NULL,           PREC_NONE},
    [TK_CONST]      = {NULL,              NULL,           PREC_NONE},
    [TK_IF]         = {NULL,              NULL,           PREC_NONE},
    [TK_ELSE]       = {NULL,              NULL,           PREC_NONE},
    [TK_WHILE]      = {NULL,              NULL,           PREC_NONE},
    [TK_FOR]        = {NULL,              NULL,           PREC_NONE},
    [TK_RETURN]     = {NULL,              NULL,           PREC_NONE},
    [TK_NULL]       = {xr_parse_literal,  NULL,           PREC_NONE},
    [TK_TRUE]       = {xr_parse_literal,  NULL,           PREC_NONE},
    [TK_FALSE]      = {xr_parse_literal,  NULL,           PREC_NONE},
    [TK_CLASS]      = {NULL,              NULL,           PREC_NONE},
    [TK_FUNCTION]   = {NULL,              NULL,           PREC_NONE},
    [TK_NEW]        = {NULL,              NULL,           PREC_NONE},
    [TK_THIS]       = {NULL,              NULL,           PREC_NONE},
    
    /* 字面量和标识符 */
    [TK_INT]        = {xr_parse_literal,  NULL,           PREC_NONE},
    [TK_FLOAT]      = {xr_parse_literal,  NULL,           PREC_NONE},
    [TK_STRING]     = {xr_parse_literal,  NULL,           PREC_NONE},
    [TK_NAME]       = {xr_parse_variable, NULL,           PREC_NONE},
    
    /* 特殊 */
    [TK_EOF]        = {NULL,              NULL,           PREC_NONE},
    [TK_ERROR]      = {NULL,              NULL,           PREC_NONE},
};

/* ========== 辅助函数 ========== */

/*
** 获取 Token 的解析规则
*/
const ParseRule *xr_get_rule(TokenType type) {
    return &rules[type];
}

/* ========== Token 操作函数 ========== */

/*
** 前进到下一个 Token
*/
void xr_parser_advance(Parser *parser) {
    parser->previous = parser->current;
    
    /* 跳过错误 Token，直到找到有效 Token */
    while (1) {
        parser->current = xr_scanner_scan(&parser->scanner);
        if (parser->current.type != TK_ERROR) break;
        
        /* 处理扫描错误 */
        xr_parser_error_at_current(parser, "意外的字符");
    }
}

/*
** 检查当前 Token 是否为指定类型
*/
int xr_parser_check(Parser *parser, TokenType type) {
    return parser->current.type == type;
}

/*
** 如果当前 Token 为指定类型，则消费它并返回 true
** 否则返回 false
*/
int xr_parser_match(Parser *parser, TokenType type) {
    if (!xr_parser_check(parser, type)) return 0;
    xr_parser_advance(parser);
    return 1;
}

/*
** 消费指定类型的 Token，如果类型不匹配则报错
*/
void xr_parser_consume(Parser *parser, TokenType type, const char *message) {
    if (parser->current.type == type) {
        xr_parser_advance(parser);
        return;
    }
    
    xr_parser_error_at_current(parser, message);
}

/* ========== 错误处理函数 ========== */

/*
** 在当前 Token 位置报告错误
*/
void xr_parser_error_at_current(Parser *parser, const char *message) {
    Token *token = &parser->current;
    if (parser->panic_mode) return;  /* 避免错误雪崩 */
    
    parser->panic_mode = 1;
    
    fprintf(stderr, "语法错误[行 %d]: ", token->line);
    
    if (token->type == TK_EOF) {
        fprintf(stderr, "文件结尾处");
    } else if (token->type == TK_ERROR) {
        /* 词法错误，不需要额外处理 */
    } else {
        fprintf(stderr, "'%.*s'", token->length, token->start);
    }
    
    fprintf(stderr, " %s\n", message);
    parser->had_error = 1;
}

/*
** 在前一个 Token 位置报告错误
*/
void xr_parser_error_at_previous(Parser *parser, const char *message) {
    Token *token = &parser->previous;
    if (parser->panic_mode) return;
    
    parser->panic_mode = 1;
    
    fprintf(stderr, "语法错误[行 %d]: ", token->line);
    
    if (token->type == TK_EOF) {
        fprintf(stderr, "文件结尾处");
    } else if (token->type == TK_ERROR) {
        /* 不处理 */
    } else {
        fprintf(stderr, "'%.*s'", token->length, token->start);
    }
    
    fprintf(stderr, " %s\n", message);
    parser->had_error = 1;
}

/*
** 在当前位置报告错误（简化版本）
*/
void xr_parser_error(Parser *parser, const char *message) {
    xr_parser_error_at_current(parser, message);
}

/*
** 错误恢复：同步到下一个语句边界
*/
void xr_parser_synchronize(Parser *parser) {
    parser->panic_mode = 0;
    
    while (parser->current.type != TK_EOF) {
        if (parser->previous.type == TK_SEMICOLON) return;
        
        switch (parser->current.type) {
            case TK_CLASS:
            case TK_FUNCTION:
            case TK_LET:
            case TK_CONST:
            case TK_FOR:
            case TK_IF:
            case TK_WHILE:
            case TK_RETURN:
                return;
            default:
                break;
        }
        
        xr_parser_advance(parser);
    }
}

/* ========== 表达式解析函数 ========== */

/*
** Pratt 解析器核心：按优先级解析表达式
** 这是 Pratt 解析器的精髓所在
*/
AstNode *xr_parse_precedence(Parser *parser, Precedence precedence) {
    /* 获取前缀解析函数 */
    xr_parser_advance(parser);
    PrefixParseFn prefix_rule = xr_get_rule(parser->previous.type)->prefix;
    if (prefix_rule == NULL) {
        xr_parser_error(parser, "期望表达式");
        return NULL;
    }
    
    /* 解析前缀表达式 */
    AstNode *left = prefix_rule(parser);
    
    /* 处理中缀表达式（根据优先级） */
    while (precedence <= xr_get_rule(parser->current.type)->precedence) {
        xr_parser_advance(parser);
        InfixParseFn infix_rule = xr_get_rule(parser->previous.type)->infix;
        left = infix_rule(parser, left);
    }
    
    return left;
}

/*
** 解析表达式（入口函数）
*/
AstNode *xr_parse_expression(Parser *parser) {
    return xr_parse_precedence(parser, PREC_ASSIGNMENT);  /* 从最低优先级开始（包括赋值） */
}

/* ========== 前缀解析函数 ========== */

/*
** 解析字面量（数字、字符串、bool、null）
*/
AstNode *xr_parse_literal(Parser *parser) {
    switch (parser->previous.type) {
        case TK_INT: {
            /* 解析整数 */
            char *end;
            xr_Integer value = strtoll(parser->previous.start, &end, 10);
            return xr_ast_literal_int(parser->X, value, parser->previous.line);
        }
        
        case TK_FLOAT: {
            /* 解析浮点数 */
            char *end;
            xr_Number value = strtod(parser->previous.start, &end);
            return xr_ast_literal_float(parser->X, value, parser->previous.line);
        }
        
        case TK_STRING: {
            /* 解析字符串（去掉引号） */
            char *str = (char *)malloc(parser->previous.length - 1);
            memcpy(str, parser->previous.start + 1, parser->previous.length - 2);
            str[parser->previous.length - 2] = '\0';
            AstNode *node = xr_ast_literal_string(parser->X, str, parser->previous.line);
            free(str);
            return node;
        }
        
        case TK_TRUE:
            return xr_ast_literal_bool(parser->X, 1, parser->previous.line);
            
        case TK_FALSE:
            return xr_ast_literal_bool(parser->X, 0, parser->previous.line);
            
        case TK_NULL:
            return xr_ast_literal_null(parser->X, parser->previous.line);
            
        default:
            xr_parser_error(parser, "未知的字面量类型");
            return NULL;
    }
}

/*
** 解析括号表达式：(expression)
*/
AstNode *xr_parse_grouping(Parser *parser) {
    AstNode *expr = xr_parse_expression(parser);
    xr_parser_consume(parser, TK_RPAREN, "期望 ')' 来结束分组表达式");
    return xr_ast_grouping(parser->X, expr, parser->previous.line);
}

/*
** 解析一元运算符：-expr, !expr
*/
AstNode *xr_parse_unary(Parser *parser) {
    TokenType operator_type = parser->previous.type;
    int line = parser->previous.line;
    
    /* 解析操作数（右结合） */
    AstNode *operand = xr_parse_precedence(parser, PREC_UNARY);
    
    /* 创建一元运算 AST 节点 */
    switch (operator_type) {
        case TK_MINUS:
            return xr_ast_unary(parser->X, AST_UNARY_NEG, operand, line);
        case TK_NOT:
            return xr_ast_unary(parser->X, AST_UNARY_NOT, operand, line);
        default:
            xr_parser_error(parser, "未知的一元运算符");
            return NULL;
    }
}

/* ========== 中缀解析函数 ========== */

/*
** 解析二元运算符：left op right
*/
AstNode *xr_parse_binary(Parser *parser, AstNode *left) {
    TokenType operator_type = parser->previous.type;
    int line = parser->previous.line;
    
    /* 获取运算符优先级 */
    const ParseRule *rule = xr_get_rule(operator_type);
    
    /* 解析右操作数（左结合：优先级 + 1） */
    AstNode *right = xr_parse_precedence(parser, rule->precedence + 1);
    
    /* 创建二元运算 AST 节点 */
    AstNodeType ast_type;
    switch (operator_type) {
        case TK_PLUS:    ast_type = AST_BINARY_ADD; break;
        case TK_MINUS:   ast_type = AST_BINARY_SUB; break;
        case TK_STAR:    ast_type = AST_BINARY_MUL; break;
        case TK_SLASH:   ast_type = AST_BINARY_DIV; break;
        case TK_PERCENT: ast_type = AST_BINARY_MOD; break;
        case TK_EQ:      ast_type = AST_BINARY_EQ; break;
        case TK_NE:      ast_type = AST_BINARY_NE; break;
        case TK_LT:      ast_type = AST_BINARY_LT; break;
        case TK_LE:      ast_type = AST_BINARY_LE; break;
        case TK_GT:      ast_type = AST_BINARY_GT; break;
        case TK_GE:      ast_type = AST_BINARY_GE; break;
        case TK_AND:     ast_type = AST_BINARY_AND; break;
        case TK_OR:      ast_type = AST_BINARY_OR; break;
        default:
            xr_parser_error(parser, "未知的二元运算符");
            return NULL;
    }
    
    return xr_ast_binary(parser->X, ast_type, left, right, line);
}

/* ========== 语句解析函数 ========== */

/*
** 解析表达式语句：expression
*/
AstNode *xr_parse_expr_statement(Parser *parser) {
    AstNode *expr = xr_parse_expression(parser);
    return xr_ast_expr_stmt(parser->X, expr, parser->previous.line);
}

/*
** 解析 print 语句：print expression
*/
AstNode *xr_parse_print_statement(Parser *parser) {
    AstNode *expr = xr_parse_expression(parser);
    return xr_ast_print_stmt(parser->X, expr, parser->previous.line);
}

/*
** 解析语句
*/
AstNode *xr_parse_statement(Parser *parser) {
    /* 控制流语句 */
    if (parser->current.type == TK_IF) {
        return xr_parse_if_statement(parser);
    }
    if (parser->current.type == TK_WHILE) {
        return xr_parse_while_statement(parser);
    }
    if (parser->current.type == TK_FOR) {
        return xr_parse_for_statement(parser);
    }
    if (parser->current.type == TK_BREAK) {
        return xr_parse_break_statement(parser);
    }
    if (parser->current.type == TK_CONTINUE) {
        return xr_parse_continue_statement(parser);
    }
    
    /* 代码块 */
    if (parser->current.type == TK_LBRACE) {
        return xr_parse_block(parser);
    }
    
    /* 暂时硬编码识别 print 关键字（作为 NAME token） */
    if (parser->current.type == TK_NAME && 
        parser->current.length == 5 &&
        memcmp(parser->current.start, "print", 5) == 0) {
        xr_parser_advance(parser);  /* 消费 "print" */
        xr_parser_consume(parser, TK_LPAREN, "期望 '(' 在 print 后");
        AstNode *stmt = xr_parse_print_statement(parser);
        xr_parser_consume(parser, TK_RPAREN, "期望 ')' 在 print 表达式后");
        return stmt;
    }
    
    /* 默认解析为表达式语句 */
    return xr_parse_expr_statement(parser);
}

/* ========== 解析器主要接口 ========== */

/*
** 初始化解析器
*/
void xr_parser_init(Parser *parser, XrayState *X, const char *source) {
    parser->X = X;
    parser->had_error = 0;
    parser->panic_mode = 0;
    
    /* 初始化扫描器 */
    xr_scanner_init(&parser->scanner, source);
    
    /* 预读第一个 Token */
    parser->current.type = TK_ERROR;
    parser->previous.type = TK_ERROR;
}

/*
** 解析源代码，返回 AST（主要入口函数）
*/
AstNode *xr_parse(XrayState *X, const char *source) {
    Parser parser;
    xr_parser_init(&parser, X, source);
    
    /* 创建程序根节点 */
    AstNode *program = xr_ast_program(X);
    
    /* 预读第一个 Token */
    xr_parser_advance(&parser);
    
    /* 解析声明列表，直到文件结束 */
    while (!xr_parser_check(&parser, TK_EOF)) {
        if (parser.panic_mode) {
            xr_parser_synchronize(&parser);
        }
        
        AstNode *decl = xr_parse_declaration(&parser);
        if (decl != NULL) {
            xr_ast_program_add(X, program, decl);
        }
        
        /* 如果有错误，停止解析 */
        if (parser.had_error) break;
    }
    
    /* 如果有错误，释放 AST 并返回 NULL */
    if (parser.had_error) {
        xr_ast_free(X, program);
        return NULL;
    }
    
    return program;
}

/* ========== 变量相关解析函数 ========== */

/*
** 解析变量引用：x
** 这是一个前缀解析函数
*/
AstNode *xr_parse_variable(Parser *parser) {
    /* parser->previous 是变量名 Token */
    char *name = (char *)malloc(parser->previous.length + 1);
    memcpy(name, parser->previous.start, parser->previous.length);
    name[parser->previous.length] = '\0';
    
    AstNode *node = xr_ast_variable(parser->X, name, parser->previous.line);
    free(name);
    return node;
}

/*
** 解析赋值：x = expression
** 这是一个中缀解析函数，left 是变量引用节点
*/
AstNode *xr_parse_assignment(Parser *parser, AstNode *left) {
    /* 检查左边是否为变量引用 */
    if (left->type != AST_VARIABLE) {
        xr_parser_error(parser, "赋值目标必须是变量");
        xr_ast_free(parser->X, left);
        return NULL;
    }
    
    /* 保存变量名 */
    char *name = strdup(left->as.variable.name);
    int line = left->line;
    
    /* 释放变量引用节点（我们不再需要它） */
    xr_ast_free(parser->X, left);
    
    /* 解析赋值表达式 */
    AstNode *value = xr_parse_expression(parser);
    
    AstNode *node = xr_ast_assignment(parser->X, name, value, line);
    free(name);
    return node;
}

/*
** 解析变量声明：let x = 10 或 const PI = 3.14
** is_const: 是否为常量声明
*/
AstNode *xr_parse_var_declaration(Parser *parser, int is_const) {
    /* 期望变量名 */
    xr_parser_consume(parser, TK_NAME, "期望变量名");
    
    /* 保存变量名 */
    char *name = (char *)malloc(parser->previous.length + 1);
    memcpy(name, parser->previous.start, parser->previous.length);
    name[parser->previous.length] = '\0';
    int line = parser->previous.line;
    
    AstNode *initializer = NULL;
    
    /* 检查是否有初始化表达式 */
    if (xr_parser_match(parser, TK_ASSIGN)) {
        /* 解析初始化表达式 */
        initializer = xr_parse_expression(parser);
    } else if (is_const) {
        /* 常量必须初始化 */
        xr_parser_error(parser, "常量必须初始化");
        free(name);
        return NULL;
    }
    
    AstNode *node = xr_ast_var_decl(parser->X, name, initializer, is_const, line);
    free(name);
    return node;
}

/*
** 解析代码块：{ ... }
*/
AstNode *xr_parse_block(Parser *parser) {
    int line = parser->previous.line;
    AstNode *block = xr_ast_block(parser->X, line);
    
    /* 解析代码块中的声明，直到遇到 } */
    while (!xr_parser_check(parser, TK_RBRACE) && !xr_parser_check(parser, TK_EOF)) {
        AstNode *decl = xr_parse_declaration(parser);
        if (decl != NULL) {
            xr_ast_block_add(parser->X, block, decl);
        }
        
        if (parser->had_error) break;
    }
    
    xr_parser_consume(parser, TK_RBRACE, "期望 '}' 在代码块结束");
    
    return block;
}

/* ========== 控制流语句解析 ========== */

/*
** 解析 if 语句
** if (condition) { ... } else if (...) { ... } else { ... }
*/
AstNode *xr_parse_if_statement(Parser *parser) {
    int line = parser->previous.line;
    xr_parser_advance(parser);  /* 消费 'if' */
    
    /* 解析条件 */
    xr_parser_consume(parser, TK_LPAREN, "期望 '(' 在 if 后");
    AstNode *condition = xr_parse_expression(parser);
    xr_parser_consume(parser, TK_RPAREN, "期望 ')' 在 if 条件后");
    
    /* 解析 then 分支（必须是 block） */
    if (!xr_parser_check(parser, TK_LBRACE)) {
        xr_parser_error_at_current(parser, "if 语句后面必须使用花括号 { }");
        return NULL;
    }
    xr_parser_advance(parser);  /* 消费 '{' */
    AstNode *then_branch = xr_parse_block(parser);
    
    /* 解析 else 分支（可选） */
    AstNode *else_branch = NULL;
    if (xr_parser_match(parser, TK_ELSE)) {
        /* else 后面可以是 block 或 if（else if） */
        if (xr_parser_check(parser, TK_IF)) {
            /* else if */
            else_branch = xr_parse_if_statement(parser);
        } else {
            /* else block */
            if (!xr_parser_check(parser, TK_LBRACE)) {
                xr_parser_error_at_current(parser, "else 后面必须使用花括号 { } 或 if 语句");
                return NULL;
            }
            xr_parser_advance(parser);  /* 消费 '{' */
            else_branch = xr_parse_block(parser);
        }
    }
    
    return xr_ast_if_stmt(parser->X, condition, then_branch, else_branch, line);
}

/*
** 解析 while 循环
** while (condition) { ... }
*/
AstNode *xr_parse_while_statement(Parser *parser) {
    int line = parser->previous.line;
    xr_parser_advance(parser);  /* 消费 'while' */
    
    /* 解析条件 */
    xr_parser_consume(parser, TK_LPAREN, "期望 '(' 在 while 后");
    AstNode *condition = xr_parse_expression(parser);
    xr_parser_consume(parser, TK_RPAREN, "期望 ')' 在 while 条件后");
    
    /* 解析循环体（必须是 block） */
    if (!xr_parser_check(parser, TK_LBRACE)) {
        xr_parser_error_at_current(parser, "while 语句后面必须使用花括号 { }");
        return NULL;
    }
    xr_parser_advance(parser);  /* 消费 '{' */
    AstNode *body = xr_parse_block(parser);
    
    return xr_ast_while_stmt(parser->X, condition, body, line);
}

/*
** 解析 for 循环
** for (init; condition; increment) { ... }
*/
AstNode *xr_parse_for_statement(Parser *parser) {
    int line = parser->previous.line;
    xr_parser_advance(parser);  /* 消费 'for' */
    
    xr_parser_consume(parser, TK_LPAREN, "期望 '(' 在 for 后");
    
    /* 解析初始化（可选） */
    AstNode *initializer = NULL;
    if (xr_parser_match(parser, TK_SEMICOLON)) {
        /* 省略初始化 */
        initializer = NULL;
    } else if (xr_parser_match(parser, TK_LET)) {
        /* let 声明 */
        initializer = xr_parse_var_declaration(parser, 0);
        xr_parser_consume(parser, TK_SEMICOLON, "期望 ';' 在 for 循环初始化后");
    } else {
        /* 表达式 */
        initializer = xr_parse_expr_statement(parser);
        xr_parser_consume(parser, TK_SEMICOLON, "期望 ';' 在 for 循环初始化后");
    }
    
    /* 解析条件（可选） */
    AstNode *condition = NULL;
    if (!xr_parser_check(parser, TK_SEMICOLON)) {
        condition = xr_parse_expression(parser);
    }
    xr_parser_consume(parser, TK_SEMICOLON, "期望 ';' 在 for 循环条件后");
    
    /* 解析更新（可选） */
    AstNode *increment = NULL;
    if (!xr_parser_check(parser, TK_RPAREN)) {
        increment = xr_parse_expression(parser);
    }
    xr_parser_consume(parser, TK_RPAREN, "期望 ')' 在 for 循环头后");
    
    /* 解析循环体（必须是 block） */
    if (!xr_parser_check(parser, TK_LBRACE)) {
        xr_parser_error_at_current(parser, "for 语句后面必须使用花括号 { }");
        return NULL;
    }
    xr_parser_advance(parser);  /* 消费 '{' */
    AstNode *body = xr_parse_block(parser);
    
    return xr_ast_for_stmt(parser->X, initializer, condition, increment, body, line);
}

/*
** 解析 break 语句
*/
AstNode *xr_parse_break_statement(Parser *parser) {
    int line = parser->previous.line;
    xr_parser_advance(parser);  /* 消费 'break' */
    return xr_ast_break_stmt(parser->X, line);
}

/*
** 解析 continue 语句
*/
AstNode *xr_parse_continue_statement(Parser *parser) {
    int line = parser->previous.line;
    xr_parser_advance(parser);  /* 消费 'continue' */
    return xr_ast_continue_stmt(parser->X, line);
}

/*
** 解析声明（变量声明、常量声明或语句）
*/
AstNode *xr_parse_declaration(Parser *parser) {
    /* 变量声明 */
    if (xr_parser_match(parser, TK_LET)) {
        return xr_parse_var_declaration(parser, 0);  /* 0 表示可变变量 */
    }
    
    /* 常量声明 */
    if (xr_parser_match(parser, TK_CONST)) {
        return xr_parse_var_declaration(parser, 1);  /* 1 表示常量 */
    }
    
    /* 代码块 */
    if (xr_parser_match(parser, TK_LBRACE)) {
        return xr_parse_block(parser);
    }
    
    /* 否则解析为语句 */
    return xr_parse_statement(parser);
}

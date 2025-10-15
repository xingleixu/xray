/*
** xparse.c
** Xray 语法解析器实现
** 使用 Pratt 解析器（上下文无关优先级解析）
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xparse.h"

/* ========== 前向声明 ========== */

static AstNode *xr_parse_arrow_function_body(Parser *parser, char **params, int param_count, int line);
static AstNode *xr_parse_template_string(Parser *parser);

/* ========== 解析规则表 ========== */

/*
** 解析规则表
** 为每种 Token 类型定义解析规则
** 这是 Pratt 解析器的核心数据结构
*/
static ParseRule rules[] = {
    /* Token 类型                前缀函数        中缀函数        优先级 */
    [TK_LPAREN]     = {xr_parse_grouping, xr_parse_call_expr, PREC_CALL},
    [TK_RPAREN]     = {NULL,              NULL,           PREC_NONE},
    [TK_LBRACE]     = {NULL,              NULL,           PREC_NONE},
    [TK_RBRACE]     = {NULL,              NULL,           PREC_NONE},
    [TK_LBRACKET]   = {xr_parse_array_literal, xr_parse_index_access, PREC_CALL},  
    [TK_RBRACKET]   = {NULL,              NULL,           PREC_NONE},
    [TK_COMMA]      = {NULL,              NULL,           PREC_NONE},
    [TK_DOT]        = {NULL,              xr_parse_member_access, PREC_CALL},  
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
    [TK_TEMPLATE_STRING] = {xr_parse_template_string, NULL, PREC_NONE},
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
            /* 解析字符串（去掉引号，处理转义序列）*/
            /* v0.10.0: 支持转义序列 \n \r \t \\ \" 等 */
            const char *src = parser->previous.start + 1;  /* 跳过开头的 " */
            size_t src_len = parser->previous.length - 2;  /* 去掉两端的引号 */
            
            char *str = (char *)malloc(src_len + 1);
            size_t dst_pos = 0;
            
            for (size_t i = 0; i < src_len; i++) {
                if (src[i] == '\\' && i + 1 < src_len) {
                    /* 处理转义序列 */
                    i++;  /* 跳过反斜杠 */
                    switch (src[i]) {
                        case 'n':  str[dst_pos++] = '\n'; break;  /* 换行 */
                        case 'r':  str[dst_pos++] = '\r'; break;  /* 回车 */
                        case 't':  str[dst_pos++] = '\t'; break;  /* 制表符 */
                        case '\\': str[dst_pos++] = '\\'; break;  /* 反斜杠 */
                        case '"':  str[dst_pos++] = '"';  break;  /* 双引号 */
                        case '\'': str[dst_pos++] = '\''; break;  /* 单引号 */
                        case 'b':  str[dst_pos++] = '\b'; break;  /* 退格 */
                        case 'f':  str[dst_pos++] = '\f'; break;  /* 换页 */
                        case '0':  str[dst_pos++] = '\0'; break;  /* 空字符 */
                        default:
                            /* 无效的转义序列，保留原样 */
                            str[dst_pos++] = '\\';
                            str[dst_pos++] = src[i];
                            break;
                    }
                } else {
                    str[dst_pos++] = src[i];
                }
            }
            str[dst_pos] = '\0';
            
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
** 解析模板字符串（v0.10.0 Day 5-6新增）
** 语法: `Hello, ${name}! ${age}`
** 
** 实现策略：
** 1. 解析反引号内的完整内容
** 2. 识别 ${...} 插值表达式
** 3. 分离字符串片段和表达式
** 4. 创建AST节点数组
*/
static AstNode *xr_parse_template_string(Parser *parser) {
    const char *template = parser->previous.start + 1;  /* 跳过开头的 ` */
    int template_len = parser->previous.length - 2;     /* 去掉两端的 ` */
    
    /* parts数组：存储字符串片段和表达式节点 */
    AstNode **parts = NULL;
    int part_count = 0;
    int part_capacity = 4;
    
    parts = (AstNode**)malloc(sizeof(AstNode*) * part_capacity);
    if (!parts) {
        xr_parser_error(parser, "内存分配失败");
        return NULL;
    }
    
    int i = 0;
    while (i < template_len) {
        /* 查找下一个 ${ */
        int expr_start = -1;
        for (int j = i; j < template_len - 1; j++) {
            if (template[j] == '$' && template[j + 1] == '{') {
                expr_start = j;
                break;
            }
        }
        
        if (expr_start == -1) {
            /* 没有更多插值，剩余部分都是字符串 */
            if (i < template_len) {
                char *str_part = (char*)malloc(template_len - i + 1);
                memcpy(str_part, template + i, template_len - i);
                str_part[template_len - i] = '\0';
                
                /* 创建字符串字面量节点 */
                AstNode *str_node = xr_ast_literal_string(parser->X, str_part, parser->previous.line);
                free(str_part);
                
                /* 添加到parts */
                if (part_count >= part_capacity) {
                    part_capacity *= 2;
                    parts = (AstNode**)realloc(parts, sizeof(AstNode*) * part_capacity);
                }
                parts[part_count++] = str_node;
            }
            break;
        }
        
        /* 添加 ${ 之前的字符串片段 */
        if (expr_start > i) {
            char *str_part = (char*)malloc(expr_start - i + 1);
            memcpy(str_part, template + i, expr_start - i);
            str_part[expr_start - i] = '\0';
            
            AstNode *str_node = xr_ast_literal_string(parser->X, str_part, parser->previous.line);
            free(str_part);
            
            if (part_count >= part_capacity) {
                part_capacity *= 2;
                parts = (AstNode**)realloc(parts, sizeof(AstNode*) * part_capacity);
            }
            parts[part_count++] = str_node;
        }
        
        /* 查找匹配的 } */
        int expr_end = -1;
        int brace_count = 1;
        int j = expr_start + 2;  /* 跳过 ${ */
        
        while (j < template_len && brace_count > 0) {
            if (template[j] == '{') {
                brace_count++;
            } else if (template[j] == '}') {
                brace_count--;
                if (brace_count == 0) {
                    expr_end = j;
                    break;
                }
            }
            j++;
        }
        
        if (expr_end == -1) {
            free(parts);
            xr_parser_error(parser, "模板字符串中缺少匹配的 }");
            return NULL;
        }
        
        /* 解析插值表达式 */
        int expr_len = expr_end - (expr_start + 2);
        if (expr_len > 0) {
            char *expr_code = (char*)malloc(expr_len + 1);
            memcpy(expr_code, template + expr_start + 2, expr_len);
            expr_code[expr_len] = '\0';
            
            /* 创建临时解析器解析表达式 */
            Scanner expr_scanner;
            xr_scanner_init(&expr_scanner, expr_code);
            
            Parser expr_parser;
            expr_parser.scanner = expr_scanner;  /* 直接赋值，不用& */
            expr_parser.X = parser->X;
            expr_parser.had_error = 0;
            expr_parser.panic_mode = 0;
            
            xr_parser_advance(&expr_parser);
            AstNode *expr_node = xr_parse_expression(&expr_parser);
            
            free(expr_code);
            
            if (expr_node) {
                if (part_count >= part_capacity) {
                    part_capacity *= 2;
                    parts = (AstNode**)realloc(parts, sizeof(AstNode*) * part_capacity);
                }
                parts[part_count++] = expr_node;
            }
        }
        
        i = expr_end + 1;  /* 跳过 } */
    }
    
    /* 如果没有任何parts，返回空字符串 */
    if (part_count == 0) {
        free(parts);
        return xr_ast_literal_string(parser->X, "", parser->previous.line);
    }
    
    /* 创建模板字符串节点 */
    AstNode *node = xr_ast_template_string(parser->X, parts, part_count, parser->previous.line);
    free(parts);  /* xr_ast_template_string内部会复制 */
    
    return node;
}

/*
** 解析括号表达式：(expression)
*/
AstNode *xr_parse_grouping(Parser *parser) {
    int line = parser->previous.line;
    
    /* ===== 第八阶段新增：检测箭头函数 ===== */
    /* 尝试解析为箭头函数的参数列表 */
    
    /* 情况1: () => expr (无参数箭头函数) */
    if (xr_parser_check(parser, TK_RPAREN)) {
        xr_parser_advance(parser);  /* 消耗 ) */
        if (xr_parser_match(parser, TK_ARROW)) {
            /* 解析箭头函数体 */
            return xr_parse_arrow_function_body(parser, NULL, 0, line);
        }
        /* 否则是语法错误：空的括号表达式 */
        xr_parser_error(parser, "空的括号表达式");
        return NULL;
    }
    
    /* 情况2: (id) => expr 或 (id, id, ...) => expr (参数列表) */
    /* 检查第一个token是否是标识符 */
    if (xr_parser_check(parser, TK_NAME)) {
        /* 保存当前位置 */
        Token first_name = parser->current;
        xr_parser_advance(parser);
        
        /* 检查后续：逗号或右括号 */
        if (xr_parser_check(parser, TK_COMMA) || xr_parser_check(parser, TK_RPAREN)) {
            /* 可能是箭头函数！收集所有参数 */
            char **params = (char**)malloc(sizeof(char*) * 10);  /* 最多10个参数 */
            int param_count = 0;
            
            /* 第一个参数 */
            params[param_count] = (char*)malloc(first_name.length + 1);
            strncpy(params[param_count], first_name.start, first_name.length);
            params[param_count][first_name.length] = '\0';
            param_count++;
            
            /* 解析剩余参数 */
            while (xr_parser_match(parser, TK_COMMA)) {
                xr_parser_consume(parser, TK_NAME, "期望参数名");
                Token param = parser->previous;
                params[param_count] = (char*)malloc(param.length + 1);
                strncpy(params[param_count], param.start, param.length);
                params[param_count][param.length] = '\0';
                param_count++;
            }
            
            /* 期望右括号 */
            if (!xr_parser_match(parser, TK_RPAREN)) {
                /* 不是箭头函数，释放参数并按普通表达式解析 */
                for (int i = 0; i < param_count; i++) free(params[i]);
                free(params);
                /* TODO: 这里需要回退，暂时报错 */
                xr_parser_error(parser, "期望 ')' 或 '=>'");
                return NULL;
            }
            
            /* 检查是否有箭头 */
            if (xr_parser_match(parser, TK_ARROW)) {
                /* 是箭头函数！解析函数体 */
                return xr_parse_arrow_function_body(parser, params, param_count, line);
            }
            
            /* 不是箭头函数，释放参数，按普通表达式解析 */
            for (int i = 0; i < param_count; i++) free(params[i]);
            free(params);
            /* TODO: 回退并重新解析 */
            xr_parser_error(parser, "暂不支持元组表达式");
            return NULL;
        }
    }
    
    /* 否则是普通的分组表达式 */
    AstNode *expr = xr_parse_expression(parser);
    xr_parser_consume(parser, TK_RPAREN, "期望 ')' 来结束分组表达式");
    return xr_ast_grouping(parser->X, expr, parser->previous.line);
}

/*
** 解析箭头函数体
** 支持两种形式：
**   1. 表达式体：=> expr（自动添加 return）
**   2. 块体：=> { ... }
*/
AstNode *xr_parse_arrow_function_body(Parser *parser, char **params, int param_count, int line) {
    AstNode *body;
    
    if (xr_parser_match(parser, TK_LBRACE)) {
        /* 块体：=> { ... } */
        body = xr_parse_block(parser);
    } else {
        /* 表达式体：=> expr */
        AstNode *expr = xr_parse_expression(parser);
        
        /* 自动包装成 return 语句 */
        AstNode *return_stmt = xr_ast_return_stmt(parser->X, expr, expr->line);
        
        /* 创建只包含一个return语句的block */
        body = xr_ast_block(parser->X, line);
        xr_ast_block_add(parser->X, body, return_stmt);
    }
    
    /* 创建函数表达式节点 */
    AstNode *func_expr = xr_ast_function_expr(parser->X, params, param_count, body, line);
    
    /* 释放参数数组（已被复制到AST节点中） */
    if (params) {
        for (int i = 0; i < param_count; i++) {
            free(params[i]);
        }
        free(params);
    }
    
    return func_expr;
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
    if (parser->current.type == TK_RETURN) {
        return xr_parse_return_statement(parser);
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
    int line = left->line;
    
    /* 变量赋值：x = 10 */
    if (left->type == AST_VARIABLE) {
        /* 保存变量名 */
        char *name = strdup(left->as.variable.name);
        
        /* 释放变量引用节点（我们不再需要它） */
        xr_ast_free(parser->X, left);
        
        /* 解析赋值表达式 */
        AstNode *value = xr_parse_expression(parser);
        
        AstNode *node = xr_ast_assignment(parser->X, name, value, line);
        free(name);
        return node;
    }
    /* 索引赋值：arr[0] = 10 */
    else if (left->type == AST_INDEX_GET) {
        /* 保存数组和索引节点 */
        AstNode *array = left->as.index_get.array;
        AstNode *index = left->as.index_get.index;
        
        /* 解析赋值表达式 */
        AstNode *value = xr_parse_expression(parser);
        
        /* 创建索引赋值节点 */
        AstNode *node = xr_ast_index_set(parser->X, array, index, value, line);
        
        /* 释放原 index_get 节点（但不释放其子节点） */
        free(left);
        
        return node;
    }
    /* 无效的赋值目标 */
    else {
        xr_parser_error(parser, "赋值目标必须是变量或数组索引");
        xr_ast_free(parser->X, left);
        return NULL;
    }
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

/* ========== 函数相关解析 ========== */

/*
** 解析函数声明
** function add(a, b) { return a + b }
*/
AstNode *xr_parse_function_declaration(Parser *parser) {
    int line = parser->previous.line;
    
    /* 解析函数名 */
    xr_parser_consume(parser, TK_NAME, "期望函数名");
    Token name_token = parser->previous;
    
    /* 复制函数名 */
    char *func_name = (char *)malloc(name_token.length + 1);
    memcpy(func_name, name_token.start, name_token.length);
    func_name[name_token.length] = '\0';
    
    /* 解析参数列表 */
    xr_parser_consume(parser, TK_LPAREN, "期望 '(' 在函数名后");
    
    /* 动态数组存储参数 */
    char **parameters = NULL;
    int param_count = 0;
    int param_capacity = 0;
    
    /* 解析参数 */
    if (!xr_parser_check(parser, TK_RPAREN)) {
        do {
            /* 扩容 */
            if (param_count >= param_capacity) {
                param_capacity = param_capacity == 0 ? 4 : param_capacity * 2;
                parameters = (char **)realloc(parameters, sizeof(char *) * param_capacity);
            }
            
            /* 解析参数名 */
            xr_parser_consume(parser, TK_NAME, "期望参数名");
            Token param_token = parser->previous;
            
            /* 复制参数名 */
            char *param_name = (char *)malloc(param_token.length + 1);
            memcpy(param_name, param_token.start, param_token.length);
            param_name[param_token.length] = '\0';
            
            parameters[param_count++] = param_name;
            
        } while (xr_parser_match(parser, TK_COMMA));
    }
    
    xr_parser_consume(parser, TK_RPAREN, "期望 ')' 在参数列表后");
    
    /* 解析函数体（必须是代码块） */
    xr_parser_consume(parser, TK_LBRACE, "函数体必须使用花括号 { }");
    AstNode *body = xr_parse_block(parser);
    
    /* 创建函数声明节点 */
    AstNode *func_decl = xr_ast_function_decl(parser->X, func_name, 
                                              parameters, param_count, body, line);
    
    /* 释放临时分配的函数名（AST已经复制了一份） */
    free(func_name);
    
    return func_decl;
}

/*
** 解析函数调用
** add(1, 2)
** callee: 被调用的表达式（通常是变量）
*/
AstNode *xr_parse_call_expr(Parser *parser, AstNode *callee) {
    int line = parser->previous.line;
    
    /* 动态数组存储参数 */
    AstNode **arguments = NULL;
    int arg_count = 0;
    int arg_capacity = 0;
    
    /* 解析参数列表 */
    if (!xr_parser_check(parser, TK_RPAREN)) {
        do {
            /* 扩容 */
            if (arg_count >= arg_capacity) {
                arg_capacity = arg_capacity == 0 ? 4 : arg_capacity * 2;
                arguments = (AstNode **)realloc(arguments, sizeof(AstNode *) * arg_capacity);
            }
            
            /* 解析参数表达式 */
            arguments[arg_count++] = xr_parse_expression(parser);
            
        } while (xr_parser_match(parser, TK_COMMA));
    }
    
    xr_parser_consume(parser, TK_RPAREN, "期望 ')' 在参数列表后");
    
    /* 创建函数调用节点 */
    return xr_ast_call_expr(parser->X, callee, arguments, arg_count, line);
}

/* ========== 数组解析函数========== */

/*
** 解析数组字面量（前缀）
** [1, 2, 3]
*/
AstNode *xr_parse_array_literal(Parser *parser) {
    int line = parser->previous.line;
    
    /* 已经消费了 '[' */
    
    /* 空数组 */
    if (xr_parser_match(parser, TK_RBRACKET)) {
        return xr_ast_array_literal(parser->X, NULL, 0, line);
    }
    
    /* 收集元素 */
    AstNode **elements = NULL;
    int count = 0;
    int capacity = 0;
    
    do {
        /* 扩容 */
        if (count >= capacity) {
            capacity = capacity == 0 ? 4 : capacity * 2;
            elements = (AstNode **)realloc(elements, sizeof(AstNode *) * capacity);
        }
        
        /* 解析元素表达式 */
        elements[count++] = xr_parse_expression(parser);
        
    } while (xr_parser_match(parser, TK_COMMA));
    
    /* 期望 ']' */
    xr_parser_consume(parser, TK_RBRACKET, "期望 ']' 在数组元素后");
    
    return xr_ast_array_literal(parser->X, elements, count, line);
}

/*
** 解析索引访问（中缀）
** arr[0]
*/
AstNode *xr_parse_index_access(Parser *parser, AstNode *array) {
    int line = parser->previous.line;
    
    /* 已经消费了 '[' */
    
    /* 解析索引表达式 */
    AstNode *index = xr_parse_expression(parser);
    
    /* 期望 ']' */
    xr_parser_consume(parser, TK_RBRACKET, "期望 ']' 在索引后");
    
    /* 创建索引访问节点 */
    return xr_ast_index_get(parser->X, array, index, line);
}

/*
** 解析成员访问（中缀）
** arr.length, arr.push
*/
AstNode *xr_parse_member_access(Parser *parser, AstNode *object) {
    int line = parser->previous.line;
    
    /* 已经消费了 '.' */
    
    /* 期望标识符 */
    xr_parser_consume(parser, TK_NAME, "期望成员名称");
    const char *name = parser->previous.start;
    int name_len = parser->previous.length;
    
    /* 复制成员名 */
    char *member_name = (char *)malloc(name_len + 1);
    strncpy(member_name, name, name_len);
    member_name[name_len] = '\0';
    
    /* 创建成员访问节点 */
    return xr_ast_member_access(parser->X, object, member_name, line);
}

/*
** 解析 return 语句
** return expr 或 return
*/
AstNode *xr_parse_return_statement(Parser *parser) {
    int line = parser->previous.line;
    xr_parser_advance(parser);  /* 消费 'return' */
    
    /* 检查是否有返回值 */
    AstNode *value = NULL;
    
    /* 如果不是语句结束符，解析返回值表达式 */
    /* 注意：Xray 没有分号结束符，所以我们检查是否是块结束或新语句开始 */
    if (parser->current.type != TK_RBRACE &&  /* 不是块结束 */
        parser->current.type != TK_EOF) {      /* 不是文件结束 */
        value = xr_parse_expression(parser);
    }
    
    return xr_ast_return_stmt(parser->X, value, line);
}

/*
** 解析声明（变量声明、常量声明或语句）
*/
AstNode *xr_parse_declaration(Parser *parser) {
    /* 函数声明 */
    if (xr_parser_match(parser, TK_FUNCTION)) {
        return xr_parse_function_declaration(parser);
    }
    
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

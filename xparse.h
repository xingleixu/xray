/*
** xparse.h
** Xray 语法解析器（Parser）
** 实现 Pratt 解析器，将 Token 流解析为 AST
*/

#ifndef xparse_h
#define xparse_h

#include "xray.h"
#include "xlex.h"
#include "xast.h"

/*
** 运算符优先级
** 数值越高，优先级越高
*/
typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  /* = */
    PREC_OR,          /* || */
    PREC_AND,         /* && */
    PREC_EQUALITY,    /* == != */
    PREC_COMPARISON,  /* < > <= >= */
    PREC_TERM,        /* + - */
    PREC_FACTOR,      /* * / % */
    PREC_UNARY,       /* ! - */
    PREC_CALL,        /* . () [] */
    PREC_PRIMARY      /* 字面量、括号 */
} Precedence;

/* 前向声明 */
typedef struct Parser Parser;

/*
** 解析函数类型
** prefix: 前缀解析函数（处理前缀运算符和字面量）
** infix: 中缀解析函数（处理二元运算符）
*/
typedef AstNode *(*PrefixParseFn)(Parser *parser);
typedef AstNode *(*InfixParseFn)(Parser *parser, AstNode *left);

/*
** 解析规则
** 每种 Token 类型对应的解析规则
*/
typedef struct {
    PrefixParseFn prefix;   /* 前缀解析函数 */
    InfixParseFn infix;     /* 中缀解析函数 */
    Precedence precedence;  /* 优先级 */
} ParseRule;

/*
** 解析器状态
** 管理解析过程中的所有状态信息
*/
struct Parser {
    Scanner scanner;        /* 词法扫描器 */
    Token current;          /* 当前 Token */
    Token previous;         /* 前一个 Token */
    int had_error;          /* 是否有语法错误 */
    int panic_mode;         /* 是否在恐慌模式（错误恢复） */
    XrayState *X;           /* Xray 状态 */
};

/* ========== 解析器主要接口 ========== */

/*
** 解析源代码，返回 AST
** 这是解析器的主要入口函数
*/
AstNode *xr_parse(XrayState *X, const char *source);

/*
** 初始化解析器
** parser: 解析器指针
** X: Xray 状态
** source: 源代码字符串
*/
void xr_parser_init(Parser *parser, XrayState *X, const char *source);

/* ========== 内部解析函数 ========== */

/*
** Token 操作函数
*/
void xr_parser_advance(Parser *parser);
int xr_parser_check(Parser *parser, TokenType type);
int xr_parser_match(Parser *parser, TokenType type);
void xr_parser_consume(Parser *parser, TokenType type, const char *message);

/*
** 表达式解析函数
*/
AstNode *xr_parse_expression(Parser *parser);
AstNode *xr_parse_precedence(Parser *parser, Precedence precedence);

/*
** 语句解析函数
*/
AstNode *xr_parse_declaration(Parser *parser);
AstNode *xr_parse_statement(Parser *parser);
AstNode *xr_parse_expr_statement(Parser *parser);
AstNode *xr_parse_print_statement(Parser *parser);
AstNode *xr_parse_block(Parser *parser);

/*
** 变量相关解析函数
*/
AstNode *xr_parse_var_declaration(Parser *parser, int is_const);
AstNode *xr_parse_variable(Parser *parser);
AstNode *xr_parse_assignment(Parser *parser, AstNode *left);

/*
** 获取 Token 的解析规则
*/
const ParseRule *xr_get_rule(TokenType type);

/*
** 错误处理函数
*/
void xr_parser_error(Parser *parser, const char *message);
void xr_parser_error_at_current(Parser *parser, const char *message);
void xr_parser_error_at_previous(Parser *parser, const char *message);
void xr_parser_synchronize(Parser *parser);

/* ========== 具体的解析函数 ========== */

/*
** 前缀解析函数（处理字面量和前缀运算符）
*/
AstNode *xr_parse_literal(Parser *parser);      /* 字面量：数字、字符串等 */
AstNode *xr_parse_grouping(Parser *parser);     /* 括号表达式：(expr) */
AstNode *xr_parse_unary(Parser *parser);        /* 一元运算符：-expr, !expr */

/*
** 中缀解析函数（处理二元运算符）
*/
AstNode *xr_parse_binary(Parser *parser, AstNode *left);    /* 二元运算：left op right */

#endif /* xparse_h */


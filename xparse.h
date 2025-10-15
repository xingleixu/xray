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
** 控制流语句解析函数
*/
AstNode *xr_parse_if_statement(Parser *parser);
AstNode *xr_parse_while_statement(Parser *parser);
AstNode *xr_parse_for_statement(Parser *parser);
AstNode *xr_parse_break_statement(Parser *parser);
AstNode *xr_parse_continue_statement(Parser *parser);

/*
** 变量相关解析函数
*/
AstNode *xr_parse_var_declaration(Parser *parser, int is_const);
AstNode *xr_parse_variable(Parser *parser);
AstNode *xr_parse_assignment(Parser *parser, AstNode *left);

/*
** 函数相关解析函数
*/
AstNode *xr_parse_function_declaration(Parser *parser);
AstNode *xr_parse_call_expr(Parser *parser, AstNode *callee);
AstNode *xr_parse_return_statement(Parser *parser);

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
AstNode *xr_parse_array_literal(Parser *parser);  /* 数组字面量：[1, 2, 3] */
AstNode *xr_parse_map_literal(Parser *parser);    /* Map字面量：{a: 1, b: 2} （v0.11.0）*/

/*
** 中缀解析函数（处理二元运算符）
*/
AstNode *xr_parse_binary(Parser *parser, AstNode *left);    /* 二元运算：left op right */
AstNode *xr_parse_index_access(Parser *parser, AstNode *array);  /* 索引访问：arr[0] */
AstNode *xr_parse_member_access(Parser *parser, AstNode *object);  /* 成员访问：arr.length */

/* ========== 类型解析函数（新增）========== */

/*
** 解析类型注解
** 如：int, float, string, int[], int | string, int?
*/
XrTypeInfo* xr_parse_type(Parser *parser);

/* ========== OOP解析函数（v0.12.0新增）========== */

/*
** 解析类声明
** class Dog extends Animal { fields... methods... }
*/
AstNode *xr_parse_class_declaration(Parser *parser);

/*
** 解析字段声明
** name: string 或 private age: int = 0
*/
AstNode *xr_parse_field_declaration(Parser *parser, bool *is_method_out);

/*
** 解析方法声明
** greet() { ... } 或 constructor(name) { ... }
** 
** @param name 方法名
** @param is_private 是否私有
** @param is_static 是否静态
*/
AstNode *xr_parse_method_declaration(Parser *parser, const char *name, 
                                     bool is_private, bool is_static);

/*
** 解析new表达式（前缀）
** new Dog("Rex")
*/
AstNode *xr_parse_new_expression(Parser *parser);

/*
** 解析this表达式（前缀）
** this
*/
AstNode *xr_parse_this_expression(Parser *parser);

/*
** 解析super调用（前缀）
** super.greet() 或 super(args)
*/
AstNode *xr_parse_super_expression(Parser *parser);

/* ========== 辅助函数（供内部使用）========== */

/* Token操作（简化名称供内部使用） */
#define match(p, type) xr_parser_match(p, type)
#define check(p, type) xr_parser_check(p, type)
#define consume(p, type, msg) xr_parser_consume(p, type, msg)
#define error(p, msg) xr_parser_error(p, msg)

#endif /* xparse_h */


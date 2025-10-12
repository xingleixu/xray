/*
** xlex.h
** Xray 词法分析器头文件
** 负责将源代码文本转换为 Token 流
*/

#ifndef xlex_h
#define xlex_h

#include "xray.h"

/* 
** Token 类型定义
** 包含所有 Xray 语言的词法单元
*/
typedef enum {
    /* 单字符符号 */
    TK_LPAREN = '(',    /* ( */
    TK_RPAREN = ')',    /* ) */
    TK_LBRACE = '{',    /* { */
    TK_RBRACE = '}',    /* } */
    TK_LBRACKET = '[',  /* [ */
    TK_RBRACKET = ']',  /* ] */
    TK_COMMA = ',',     /* , */
    TK_DOT = '.',       /* . */
    TK_SEMICOLON = ';', /* ; 用于 for 循环分隔符等 */
    TK_PLUS = '+',      /* + */
    TK_MINUS = '-',     /* - */
    TK_STAR = '*',      /* * */
    TK_SLASH = '/',     /* / */
    TK_PERCENT = '%',   /* % */
    
    /* 多字符符号和关键字（从 256 开始，避免与 ASCII 冲突） */
    TK_EQ = 256,        /* == */
    TK_NE,              /* != */
    TK_LT,              /* < */
    TK_LE,              /* <= */
    TK_GT,              /* > */
    TK_GE,              /* >= */
    TK_ASSIGN,          /* = */
    TK_AND,             /* && */
    TK_OR,              /* || */
    TK_NOT,             /* ! */
    
    /* 关键字 */
    TK_LET,             /* let */
    TK_CONST,           /* const */
    TK_IF,              /* if */
    TK_ELSE,            /* else */
    TK_WHILE,           /* while */
    TK_FOR,             /* for */
    TK_BREAK,           /* break */
    TK_CONTINUE,        /* continue */
    TK_RETURN,          /* return */
    TK_NULL,            /* null */
    TK_TRUE,            /* true */
    TK_FALSE,           /* false */
    TK_CLASS,           /* class */
    TK_FUNCTION,        /* function */
    TK_NEW,             /* new */
    TK_THIS,            /* this */
    
    /* 字面量和标识符 */
    TK_INT,             /* 整数字面量 */
    TK_FLOAT,           /* 浮点数字面量 */
    TK_STRING,          /* 字符串字面量 */
    TK_NAME,            /* 标识符 */
    
    /* 特殊 */
    TK_EOF,             /* 文件结束 */
    TK_ERROR            /* 错误 */
} TokenType;

/* Token 结构 */
typedef struct {
    TokenType type;     /* Token 类型 */
    const char *start;  /* Token 开始位置 */
    int length;         /* Token 长度 */
    int line;           /* 所在行号 */
} Token;

/* 词法扫描器状态 */
typedef struct {
    const char *source; /* 源代码字符串 */
    const char *start;  /* 当前 Token 开始位置 */
    const char *current; /* 当前扫描位置 */
    int line;           /* 当前行号 */
} Scanner;

/* 词法扫描器函数 */
void xr_scanner_init(Scanner *scanner, const char *source);
Token xr_scanner_scan(Scanner *scanner);
const char *xr_token_name(TokenType type);

#endif /* xlex_h */


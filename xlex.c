/*
** xlex.c
** Xray 词法分析器实现
*/

#include <string.h>
#include <ctype.h>
#include "xlex.h"

/* 
** 初始化词法扫描器
** 参数：
**   scanner - 扫描器对象
**   source  - 源代码字符串
*/
void xr_scanner_init(Scanner *scanner, const char *source) {
    scanner->source = source;
    scanner->start = source;
    scanner->current = source;
    scanner->line = 1;
}

/* 检查是否到达源代码末尾 */
static int is_at_end(Scanner *scanner) {
    return *scanner->current == '\0';
}

/* 前进一个字符并返回 */
static char advance(Scanner *scanner) {
    scanner->current++;
    return scanner->current[-1];
}

/* 查看当前字符但不前进 */
static char peek(Scanner *scanner) {
    return *scanner->current;
}

/* 查看下一个字符但不前进 */
static char peek_next(Scanner *scanner) {
    if (is_at_end(scanner)) return '\0';
    return scanner->current[1];
}

/* 如果当前字符匹配则前进 */
static int match(Scanner *scanner, char expected) {
    if (is_at_end(scanner)) return 0;
    if (*scanner->current != expected) return 0;
    scanner->current++;
    return 1;
}

/* 创建一个 Token */
static Token make_token(Scanner *scanner, TokenType type) {
    Token token;
    token.type = type;
    token.start = scanner->start;
    token.length = (int)(scanner->current - scanner->start);
    token.line = scanner->line;
    return token;
}

/* 创建一个错误 Token */
static Token error_token(Scanner *scanner, const char *message) {
    Token token;
    token.type = TK_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner->line;
    return token;
}

/* 跳过空白字符和注释 */
static void skip_whitespace(Scanner *scanner) {
    for (;;) {
        char c = peek(scanner);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(scanner);
                break;
            case '\n':
                scanner->line++;
                advance(scanner);
                break;
            case '/':
                /* 处理注释 */
                if (peek_next(scanner) == '/') {
                    /* 单行注释 // */
                    while (peek(scanner) != '\n' && !is_at_end(scanner)) {
                        advance(scanner);
                    }
                } else if (peek_next(scanner) == '*') {
                    /* 多行注释 */
                    advance(scanner); /* / */
                    advance(scanner); /* * */
                    while (!is_at_end(scanner)) {
                        if (peek(scanner) == '*' && peek_next(scanner) == '/') {
                            advance(scanner); /* * */
                            advance(scanner); /* / */
                            break;
                        }
                        if (peek(scanner) == '\n') scanner->line++;
                        advance(scanner);
                    }
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

/* 检查关键字 */
static TokenType check_keyword(Scanner *scanner, int start, int length,
                               const char *rest, TokenType type) {
    if (scanner->current - scanner->start == start + length &&
        memcmp(scanner->start + start, rest, length) == 0) {
        return type;
    }
    return TK_NAME;
}

/* 识别标识符类型（关键字或普通标识符） */
static TokenType identifier_type(Scanner *scanner) {
    switch (scanner->start[0]) {
        case 'a': return check_keyword(scanner, 1, 2, "ny", TK_ANY);
        case 'b':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'o': return check_keyword(scanner, 2, 2, "ol", TK_BOOL);
                    case 'r': return check_keyword(scanner, 2, 3, "eak", TK_BREAK);
                }
            }
            break;
        case 'c':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'l': return check_keyword(scanner, 2, 3, "ass", TK_CLASS);
                    case 'o':
                        if (scanner->current - scanner->start > 2) {
                            switch (scanner->start[2]) {
                                case 'n':
                                    if (scanner->current - scanner->start == 5) {
                                        return check_keyword(scanner, 2, 3, "nst", TK_CONST);
                                    }
                                    return check_keyword(scanner, 2, 6, "ntinue", TK_CONTINUE);
                            }
                        }
                }
            }
            break;
        case 'e': return check_keyword(scanner, 1, 3, "lse", TK_ELSE);
        case 'f':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'a': return check_keyword(scanner, 2, 3, "lse", TK_FALSE);
                    case 'l': return check_keyword(scanner, 2, 3, "oat", TK_TYPE_FLOAT);
                    case 'o': return check_keyword(scanner, 2, 1, "r", TK_FOR);
                    case 'u': return check_keyword(scanner, 2, 6, "nction", TK_FUNCTION);
                }
            }
            break;
        case 'i':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'f': return TK_IF;  /* if */
                    case 'n': return check_keyword(scanner, 2, 1, "t", TK_TYPE_INT);  /* int */
                }
            }
            break;
        case 'l': return check_keyword(scanner, 1, 2, "et", TK_LET);
        case 'n':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'e': return check_keyword(scanner, 2, 1, "w", TK_NEW);
                    case 'u': return check_keyword(scanner, 2, 2, "ll", TK_NULL);
                }
            }
            break;
        case 'r': return check_keyword(scanner, 1, 5, "eturn", TK_RETURN);
        case 's': return check_keyword(scanner, 1, 5, "tring", TK_TYPE_STRING);
        case 't':
            if (scanner->current - scanner->start > 1) {
                switch (scanner->start[1]) {
                    case 'h': return check_keyword(scanner, 2, 2, "is", TK_THIS);
                    case 'r': return check_keyword(scanner, 2, 2, "ue", TK_TRUE);
                }
            }
            break;
        case 'v': return check_keyword(scanner, 1, 3, "oid", TK_VOID);
        case 'w': return check_keyword(scanner, 1, 4, "hile", TK_WHILE);
    }
    return TK_NAME;
}

/* 扫描标识符 */
static Token identifier(Scanner *scanner) {
    while (isalnum(peek(scanner)) || peek(scanner) == '_') {
        advance(scanner);
    }
    return make_token(scanner, identifier_type(scanner));
}

/* 扫描数字 */
static Token number(Scanner *scanner) {
    TokenType type = TK_INT;
    
    /* 扫描整数部分 */
    while (isdigit(peek(scanner))) {
        advance(scanner);
    }
    
    /* 检查小数点 */
    if (peek(scanner) == '.' && isdigit(peek_next(scanner))) {
        type = TK_FLOAT;
        advance(scanner); /* 消耗 '.' */
        while (isdigit(peek(scanner))) {
            advance(scanner);
        }
    }
    
    /* 检查科学计数法 */
    if (peek(scanner) == 'e' || peek(scanner) == 'E') {
        type = TK_FLOAT;
        advance(scanner);
        if (peek(scanner) == '+' || peek(scanner) == '-') {
            advance(scanner);
        }
        while (isdigit(peek(scanner))) {
            advance(scanner);
        }
    }
    
    return make_token(scanner, type);
}

/* 
** 扫描字符串
** v0.10.0: 基本扫描，转义序列在解析阶段处理
*/
static Token string(Scanner *scanner) {
    while (peek(scanner) != '"' && !is_at_end(scanner)) {
        /* 支持转义的引号 */
        if (peek(scanner) == '\\') {
            advance(scanner);  /* 跳过 \ */
            if (!is_at_end(scanner)) {
                advance(scanner);  /* 跳过转义字符 */
            }
        } else {
            if (peek(scanner) == '\n') scanner->line++;
            advance(scanner);
        }
    }
    
    if (is_at_end(scanner)) {
        return error_token(scanner, "未结束的字符串");
    }
    
    advance(scanner); /* 消耗结束的 " */
    return make_token(scanner, TK_STRING);
}

/*
** 扫描模板字符串（v0.10.0 Day 5新增）
** 语法: `Hello, ${name}!`
** 
** 简化实现：将模板字符串作为一个整体token
** 包含完整的反引号内容，在解析阶段处理${...}插值
*/
static Token template_string(Scanner *scanner) {
    while (peek(scanner) != '`' && !is_at_end(scanner)) {
        /* 支持转义的反引号 */
        if (peek(scanner) == '\\') {
            advance(scanner);  /* 跳过 \ */
            if (!is_at_end(scanner)) {
                advance(scanner);  /* 跳过转义字符 */
            }
        } else {
            if (peek(scanner) == '\n') scanner->line++;
            advance(scanner);
        }
    }
    
    if (is_at_end(scanner)) {
        return error_token(scanner, "未结束的模板字符串");
    }
    
    advance(scanner); /* 消耗结束的 ` */
    return make_token(scanner, TK_TEMPLATE_STRING);
}

/* 
** 扫描下一个 Token
** 这是词法分析器的核心函数
*/
Token xr_scanner_scan(Scanner *scanner) {
    skip_whitespace(scanner);
    scanner->start = scanner->current;
    
    if (is_at_end(scanner)) {
        return make_token(scanner, TK_EOF);
    }
    
    char c = advance(scanner);
    
    /* 标识符和关键字 */
    if (isalpha(c) || c == '_') {
        return identifier(scanner);
    }
    
    /* 数字 */
    if (isdigit(c)) {
        return number(scanner);
    }
    
    /* 其他符号 */
    switch (c) {
        case '(': return make_token(scanner, TK_LPAREN);
        case ')': return make_token(scanner, TK_RPAREN);
        case '{': return make_token(scanner, TK_LBRACE);
        case '}': return make_token(scanner, TK_RBRACE);
        case '[': return make_token(scanner, TK_LBRACKET);
        case ']': return make_token(scanner, TK_RBRACKET);
        case ',': return make_token(scanner, TK_COMMA);
        case '.': return make_token(scanner, TK_DOT);
        case ':': return make_token(scanner, TK_COLON);  /* v0.11.0 Map字面量 */
        case ';': return make_token(scanner, TK_SEMICOLON);
        case '+': return make_token(scanner, TK_PLUS);
        case '-': return make_token(scanner, TK_MINUS);
        case '*': return make_token(scanner, TK_STAR);
        case '/': return make_token(scanner, TK_SLASH);
        case '%': return make_token(scanner, TK_PERCENT);
        case '!':
            return make_token(scanner, match(scanner, '=') ? TK_NE : TK_NOT);
        case '=':
            if (match(scanner, '=')) {
                return make_token(scanner, TK_EQ);  /* == */
            } else if (match(scanner, '>')) {
                return make_token(scanner, TK_ARROW);  /* => */
            }
            return make_token(scanner, TK_ASSIGN);  /* = */
        case '<':
            return make_token(scanner, match(scanner, '=') ? TK_LE : TK_LT);
        case '>':
            return make_token(scanner, match(scanner, '=') ? TK_GE : TK_GT);
        case '&':
            if (match(scanner, '&')) return make_token(scanner, TK_AND);
            break;
        case '|':
            if (match(scanner, '|')) {
                return make_token(scanner, TK_OR);  /* || */
            }
            return make_token(scanner, TK_PIPE);  /* | (联合类型) */
        case '?':
            return make_token(scanner, TK_QUESTION);  /* ? (可选类型) */
        case '"':
            return string(scanner);
        case '`':
            return template_string(scanner);  /* v0.10.0: 模板字符串 */
    }
    
    return error_token(scanner, "未知字符");
}

/* 
** 返回 Token 类型的名称
** 用于调试和错误信息
*/
const char *xr_token_name(TokenType type) {
    switch (type) {
        case TK_LPAREN: return "(";
        case TK_RPAREN: return ")";
        case TK_LBRACE: return "{";
        case TK_RBRACE: return "}";
        case TK_LBRACKET: return "[";
        case TK_RBRACKET: return "]";
        case TK_COMMA: return ",";
        case TK_DOT: return ".";
        case TK_SEMICOLON: return ";";
        case TK_PLUS: return "+";
        case TK_MINUS: return "-";
        case TK_STAR: return "*";
        case TK_SLASH: return "/";
        case TK_PERCENT: return "%";
        case TK_EQ: return "==";
        case TK_NE: return "!=";
        case TK_LT: return "<";
        case TK_LE: return "<=";
        case TK_GT: return ">";
        case TK_GE: return ">=";
        case TK_ASSIGN: return "=";
        case TK_AND: return "&&";
        case TK_OR: return "||";
        case TK_NOT: return "!";
        case TK_LET: return "let";
        case TK_CONST: return "const";
        case TK_IF: return "if";
        case TK_ELSE: return "else";
        case TK_WHILE: return "while";
        case TK_FOR: return "for";
        case TK_BREAK: return "break";
        case TK_CONTINUE: return "continue";
        case TK_RETURN: return "return";
        case TK_NULL: return "null";
        case TK_TRUE: return "true";
        case TK_FALSE: return "false";
        case TK_CLASS: return "class";
        case TK_FUNCTION: return "function";
        case TK_NEW: return "new";
        case TK_THIS: return "this";
        /* 类型关键字 */
        case TK_VOID: return "void";
        case TK_BOOL: return "bool";
        case TK_TYPE_INT: return "int";
        case TK_TYPE_FLOAT: return "float";
        case TK_TYPE_STRING: return "string";
        case TK_ANY: return "any";
        /* 类型运算符 */
        case TK_QUESTION: return "?";
        case TK_PIPE: return "|";
        case TK_ARROW: return "=>";
        /* 字面量 */
        case TK_INT: return "INT";
        case TK_FLOAT: return "FLOAT";
        case TK_STRING: return "STRING";
        case TK_TEMPLATE_STRING: return "TEMPLATE_STRING";
        case TK_NAME: return "NAME";
        case TK_EOF: return "EOF";
        case TK_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}


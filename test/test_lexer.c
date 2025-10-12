/*
** test_lexer.c
** 词法分析器测试程序
*/

#include <stdio.h>
#include <string.h>
#include "../xlex.h"

/* 测试计数器 */
static int test_count = 0;
static int test_passed = 0;

/* 测试断言 */
#define TEST_ASSERT(condition, message) do { \
    test_count++; \
    if (condition) { \
        test_passed++; \
        printf("  ✓ 测试 %d: %s\n", test_count, message); \
    } else { \
        printf("  ✗ 测试 %d: %s\n", test_count, message); \
    } \
} while (0)

/* 打印 Token */
static void print_token(Token token) {
    printf("    [%s] ", xr_token_name(token.type));
    if (token.type == TK_ERROR) {
        printf("\"%s\"", token.start);
    } else {
        printf("'%.*s'", token.length, token.start);
    }
    printf(" (行 %d)\n", token.line);
}

/* 测试基本符号 */
static void test_basic_symbols(void) {
    printf("\n测试基本符号:\n");
    
    Scanner scanner;
    const char *source = "( ) { } [ ] , . ; + - * / %";
    xr_scanner_init(&scanner, source);
    
    TokenType expected[] = {
        TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
        TK_LBRACKET, TK_RBRACKET, TK_COMMA, TK_DOT,
        TK_SEMICOLON, TK_PLUS, TK_MINUS, TK_STAR,
        TK_SLASH, TK_PERCENT, TK_EOF
    };
    
    for (int i = 0; expected[i] != TK_EOF; i++) {
        Token token = xr_scanner_scan(&scanner);
        print_token(token);
        TEST_ASSERT(token.type == expected[i], "符号类型正确");
    }
}

/* 测试关键字 */
static void test_keywords(void) {
    printf("\n测试关键字:\n");
    
    Scanner scanner;
    const char *source = "let const if else while for return null true false class function new this";
    xr_scanner_init(&scanner, source);
    
    TokenType expected[] = {
        TK_LET, TK_CONST, TK_IF, TK_ELSE, TK_WHILE, TK_FOR,
        TK_RETURN, TK_NULL, TK_TRUE, TK_FALSE, TK_CLASS,
        TK_FUNCTION, TK_NEW, TK_THIS, TK_EOF
    };
    
    for (int i = 0; expected[i] != TK_EOF; i++) {
        Token token = xr_scanner_scan(&scanner);
        print_token(token);
        TEST_ASSERT(token.type == expected[i], "关键字识别正确");
    }
}

/* 测试标识符 */
static void test_identifiers(void) {
    printf("\n测试标识符:\n");
    
    Scanner scanner;
    const char *source = "x abc xyz123 _private my_var";
    xr_scanner_init(&scanner, source);
    
    Token token;
    while ((token = xr_scanner_scan(&scanner)).type != TK_EOF) {
        print_token(token);
        TEST_ASSERT(token.type == TK_NAME, "标识符识别正确");
    }
}

/* 测试数字 */
static void test_numbers(void) {
    printf("\n测试数字:\n");
    
    Scanner scanner;
    const char *source = "123 456.789 3.14 1e10 2.5e-3";
    xr_scanner_init(&scanner, source);
    
    TokenType expected[] = {TK_INT, TK_FLOAT, TK_FLOAT, TK_FLOAT, TK_FLOAT, TK_EOF};
    
    for (int i = 0; expected[i] != TK_EOF; i++) {
        Token token = xr_scanner_scan(&scanner);
        print_token(token);
        TEST_ASSERT(token.type == expected[i], "数字类型正确");
    }
}

/* 测试字符串 */
static void test_strings(void) {
    printf("\n测试字符串:\n");
    
    Scanner scanner;
    const char *source = "\"hello\" \"world\" \"Hello, Xray!\"";
    xr_scanner_init(&scanner, source);
    
    Token token;
    while ((token = xr_scanner_scan(&scanner)).type != TK_EOF) {
        print_token(token);
        TEST_ASSERT(token.type == TK_STRING, "字符串识别正确");
    }
}

/* 测试运算符 */
static void test_operators(void) {
    printf("\n测试运算符:\n");
    
    Scanner scanner;
    const char *source = "== != < <= > >= = && || !";
    xr_scanner_init(&scanner, source);
    
    TokenType expected[] = {
        TK_EQ, TK_NE, TK_LT, TK_LE, TK_GT, TK_GE,
        TK_ASSIGN, TK_AND, TK_OR, TK_NOT, TK_EOF
    };
    
    for (int i = 0; expected[i] != TK_EOF; i++) {
        Token token = xr_scanner_scan(&scanner);
        print_token(token);
        TEST_ASSERT(token.type == expected[i], "运算符识别正确");
    }
}

/* 测试注释 */
static void test_comments(void) {
    printf("\n测试注释:\n");
    
    Scanner scanner;
    const char *source = 
        "let x = 10 // 单行注释\n"
        "/* 多行\n"
        "   注释 */\n"
        "let y = 20";
    xr_scanner_init(&scanner, source);
    
    Token token;
    int count = 0;
    while ((token = xr_scanner_scan(&scanner)).type != TK_EOF) {
        print_token(token);
        count++;
    }
    
    TEST_ASSERT(count == 8, "注释被正确跳过");
}

/* 测试完整程序 */
static void test_complete_program(void) {
    printf("\n测试完整程序:\n");
    
    const char *source = 
        "let x = 10\n"
        "if (x > 5) {\n"
        "    x = x + 1\n"
        "}";
    
    Scanner scanner;
    xr_scanner_init(&scanner, source);
    
    Token token;
    int count = 0;
    while ((token = xr_scanner_scan(&scanner)).type != TK_EOF) {
        print_token(token);
        count++;
    }
    
    TEST_ASSERT(count > 0, "完整程序扫描成功");
}

/* 主测试函数 */
int main(void) {
    printf("=== Xray 词法分析器测试 ===\n");
    
    test_basic_symbols();
    test_keywords();
    test_identifiers();
    test_numbers();
    test_strings();
    test_operators();
    test_comments();
    test_complete_program();
    
    printf("\n=== 测试总结 ===\n");
    printf("总测试数: %d\n", test_count);
    printf("通过: %d\n", test_passed);
    printf("失败: %d\n", test_count - test_passed);
    
    if (test_passed == test_count) {
        printf("\n✓ 所有测试通过!\n");
        return 0;
    } else {
        printf("\n✗ 有测试失败\n");
        return 1;
    }
}


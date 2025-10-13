/*
** test_parser.c
** Xray 语法解析器测试
** 测试表达式解析和 AST 生成
*/

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "../xray.h"
#include "../xparse.h"

/* 测试辅助函数 */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    printf("测试: %s ... ", name); \
    fflush(stdout);

#define PASS() \
    printf("✓ 通过\n"); \
    tests_passed++;

#define FAIL(msg) \
    printf("✗ 失败: %s\n", msg); \
    tests_failed++;

/* ========== 字面量解析测试 ========== */

/* 测试整数解析 */
static void test_parse_int() {
    TEST("整数解析");
    
    XrayState *X = xray_newstate();
    AstNode *ast = xr_parse(X, "42");
    
    assert(ast != NULL);
    assert(ast->type == AST_PROGRAM);
    assert(ast->as.program.count == 1);
    
    AstNode *stmt = ast->as.program.statements[0];
    assert(stmt->type == AST_EXPR_STMT);
    
    AstNode *expr = stmt->as.expr_stmt;
    assert(expr->type == AST_LITERAL_INT);
    assert(xr_toint(expr->as.literal.value) == 42);
    
    xr_ast_free(X, ast);
    xray_close(X);
    
    PASS();
}

/* 测试浮点数解析 */
static void test_parse_float() {
    TEST("浮点数解析");
    
    XrayState *X = xray_newstate();
    AstNode *ast = xr_parse(X, "3.14");
    
    assert(ast != NULL);
    assert(ast->type == AST_PROGRAM);
    
    AstNode *expr = ast->as.program.statements[0]->as.expr_stmt;
    assert(expr->type == AST_LITERAL_FLOAT);
    assert(xr_tofloat(expr->as.literal.value) == 3.14);
    
    xr_ast_free(X, ast);
    xray_close(X);
    
    PASS();
}

/* 测试字符串解析 */
static void test_parse_string() {
    TEST("字符串解析");
    
    XrayState *X = xray_newstate();
    AstNode *ast = xr_parse(X, "\"hello\"");
    
    assert(ast != NULL);
    assert(ast->type == AST_PROGRAM);
    
    AstNode *expr = ast->as.program.statements[0]->as.expr_stmt;
    assert(expr->type == AST_LITERAL_STRING);
    
    xr_ast_free(X, ast);
    xray_close(X);
    
    PASS();
}

/* 测试布尔值解析 */
static void test_parse_bool() {
    TEST("布尔值解析");
    
    XrayState *X = xray_newstate();
    
    /* 测试 true */
    AstNode *ast_true = xr_parse(X, "true");
    assert(ast_true != NULL);
    AstNode *expr_true = ast_true->as.program.statements[0]->as.expr_stmt;
    assert(expr_true->type == AST_LITERAL_TRUE);
    
    /* 测试 false */
    AstNode *ast_false = xr_parse(X, "false");
    assert(ast_false != NULL);
    AstNode *expr_false = ast_false->as.program.statements[0]->as.expr_stmt;
    assert(expr_false->type == AST_LITERAL_FALSE);
    
    xr_ast_free(X, ast_true);
    xr_ast_free(X, ast_false);
    xray_close(X);
    
    PASS();
}

/* 测试 null 解析 */
static void test_parse_null() {
    TEST("null 解析");
    
    XrayState *X = xray_newstate();
    AstNode *ast = xr_parse(X, "null");
    
    assert(ast != NULL);
    AstNode *expr = ast->as.program.statements[0]->as.expr_stmt;
    assert(expr->type == AST_LITERAL_NULL);
    
    xr_ast_free(X, ast);
    xray_close(X);
    
    PASS();
}

/* ========== 算术表达式测试 ========== */

/* 测试加法表达式 */
static void test_parse_add() {
    TEST("加法表达式");
    
    XrayState *X = xray_newstate();
    AstNode *ast = xr_parse(X, "1 + 2");
    
    assert(ast != NULL);
    AstNode *expr = ast->as.program.statements[0]->as.expr_stmt;
    assert(expr->type == AST_BINARY_ADD);
    
    assert(expr->as.binary.left->type == AST_LITERAL_INT);
    assert(xr_toint(expr->as.binary.left->as.literal.value) == 1);
    
    assert(expr->as.binary.right->type == AST_LITERAL_INT);
    assert(xr_toint(expr->as.binary.right->as.literal.value) == 2);
    
    xr_ast_free(X, ast);
    xray_close(X);
    
    PASS();
}

/* 测试减法表达式 */
static void test_parse_sub() {
    TEST("减法表达式");
    
    XrayState *X = xray_newstate();
    AstNode *ast = xr_parse(X, "10 - 3");
    
    assert(ast != NULL);
    AstNode *expr = ast->as.program.statements[0]->as.expr_stmt;
    assert(expr->type == AST_BINARY_SUB);
    
    xr_ast_free(X, ast);
    xray_close(X);
    
    PASS();
}

/* 测试乘法表达式 */
static void test_parse_mul() {
    TEST("乘法表达式");
    
    XrayState *X = xray_newstate();
    AstNode *ast = xr_parse(X, "4 * 5");
    
    assert(ast != NULL);
    AstNode *expr = ast->as.program.statements[0]->as.expr_stmt;
    assert(expr->type == AST_BINARY_MUL);
    
    xr_ast_free(X, ast);
    xray_close(X);
    
    PASS();
}

/* 测试除法表达式 */
static void test_parse_div() {
    TEST("除法表达式");
    
    XrayState *X = xray_newstate();
    AstNode *ast = xr_parse(X, "15 / 3");
    
    assert(ast != NULL);
    AstNode *expr = ast->as.program.statements[0]->as.expr_stmt;
    assert(expr->type == AST_BINARY_DIV);
    
    xr_ast_free(X, ast);
    xray_close(X);
    
    PASS();
}

/* 测试取模表达式 */
static void test_parse_mod() {
    TEST("取模表达式");
    
    XrayState *X = xray_newstate();
    AstNode *ast = xr_parse(X, "17 % 5");
    
    assert(ast != NULL);
    AstNode *expr = ast->as.program.statements[0]->as.expr_stmt;
    assert(expr->type == AST_BINARY_MOD);
    
    xr_ast_free(X, ast);
    xray_close(X);
    
    PASS();
}

/* ========== 优先级测试 ========== */

/* 测试运算符优先级 */
static void test_parse_precedence() {
    TEST("运算符优先级");
    
    XrayState *X = xray_newstate();
    /* 解析 2 + 3 * 4，应该解析为 2 + (3 * 4) */
    AstNode *ast = xr_parse(X, "2 + 3 * 4");
    
    assert(ast != NULL);
    AstNode *expr = ast->as.program.statements[0]->as.expr_stmt;
    
    /* 根节点应该是加法 */
    assert(expr->type == AST_BINARY_ADD);
    
    /* 左边是 2 */
    assert(expr->as.binary.left->type == AST_LITERAL_INT);
    assert(xr_toint(expr->as.binary.left->as.literal.value) == 2);
    
    /* 右边是 3 * 4 */
    assert(expr->as.binary.right->type == AST_BINARY_MUL);
    
    xr_ast_free(X, ast);
    xray_close(X);
    
    PASS();
}

/* 测试括号表达式 */
static void test_parse_parentheses() {
    TEST("括号表达式");
    
    XrayState *X = xray_newstate();
    /* 解析 (2 + 3) * 4，应该解析为 (2 + 3) * 4 */
    AstNode *ast = xr_parse(X, "(2 + 3) * 4");
    
    assert(ast != NULL);
    AstNode *expr = ast->as.program.statements[0]->as.expr_stmt;
    
    /* 根节点应该是乘法 */
    assert(expr->type == AST_BINARY_MUL);
    
    /* 左边是分组表达式 */
    assert(expr->as.binary.left->type == AST_GROUPING);
    
    /* 分组内部是加法 */
    AstNode *grouped = expr->as.binary.left->as.grouping;
    assert(grouped->type == AST_BINARY_ADD);
    
    xr_ast_free(X, ast);
    xray_close(X);
    
    PASS();
}

/* ========== 一元运算符测试 ========== */

/* 测试取负运算符 */
static void test_parse_negate() {
    TEST("取负运算符");
    
    XrayState *X = xray_newstate();
    AstNode *ast = xr_parse(X, "-5");
    
    assert(ast != NULL);
    AstNode *expr = ast->as.program.statements[0]->as.expr_stmt;
    assert(expr->type == AST_UNARY_NEG);
    
    assert(expr->as.unary.operand->type == AST_LITERAL_INT);
    assert(xr_toint(expr->as.unary.operand->as.literal.value) == 5);
    
    xr_ast_free(X, ast);
    xray_close(X);
    
    PASS();
}

/* 测试逻辑非运算符 */
static void test_parse_not() {
    TEST("逻辑非运算符");
    
    XrayState *X = xray_newstate();
    AstNode *ast = xr_parse(X, "!true");
    
    assert(ast != NULL);
    AstNode *expr = ast->as.program.statements[0]->as.expr_stmt;
    assert(expr->type == AST_UNARY_NOT);
    
    assert(expr->as.unary.operand->type == AST_LITERAL_TRUE);
    
    xr_ast_free(X, ast);
    xray_close(X);
    
    PASS();
}

/* ========== 比较运算符测试 ========== */

/* 测试比较运算符 */
static void test_parse_comparisons() {
    TEST("比较运算符");
    
    XrayState *X = xray_newstate();
    
    /* 测试各种比较运算符 */
    const char* tests[] = {
        "5 == 5", "5 != 3", "5 < 10", 
        "5 <= 5", "10 > 5", "10 >= 10"
    };
    AstNodeType expected[] = {
        AST_BINARY_EQ, AST_BINARY_NE, AST_BINARY_LT,
        AST_BINARY_LE, AST_BINARY_GT, AST_BINARY_GE
    };
    
    for (int i = 0; i < 6; i++) {
        AstNode *ast = xr_parse(X, tests[i]);
        assert(ast != NULL);
        
        AstNode *expr = ast->as.program.statements[0]->as.expr_stmt;
        assert(expr->type == expected[i]);
        
        xr_ast_free(X, ast);
    }
    
    xray_close(X);
    
    PASS();
}

/* ========== 逻辑运算符测试 ========== */

/* 测试逻辑运算符 */
static void test_parse_logical() {
    TEST("逻辑运算符");
    
    XrayState *X = xray_newstate();
    
    /* 测试逻辑与 */
    AstNode *ast_and = xr_parse(X, "true && false");
    assert(ast_and != NULL);
    AstNode *expr_and = ast_and->as.program.statements[0]->as.expr_stmt;
    assert(expr_and->type == AST_BINARY_AND);
    
    /* 测试逻辑或 */
    AstNode *ast_or = xr_parse(X, "true || false");
    assert(ast_or != NULL);
    AstNode *expr_or = ast_or->as.program.statements[0]->as.expr_stmt;
    assert(expr_or->type == AST_BINARY_OR);
    
    xr_ast_free(X, ast_and);
    xr_ast_free(X, ast_or);
    xray_close(X);
    
    PASS();
}

/* ========== 复杂表达式测试 ========== */

/* 测试复杂表达式 */
static void test_parse_complex() {
    TEST("复杂表达式");
    
    XrayState *X = xray_newstate();
    
    /* 测试复杂表达式：(1 + 2) * 3 - 4 / 2 */
    AstNode *ast = xr_parse(X, "(1 + 2) * 3 - 4 / 2");
    
    assert(ast != NULL);
    AstNode *expr = ast->as.program.statements[0]->as.expr_stmt;
    
    /* 根节点应该是减法（最低优先级） */
    assert(expr->type == AST_BINARY_SUB);
    
    /* 左边应该是 (1 + 2) * 3 */
    assert(expr->as.binary.left->type == AST_BINARY_MUL);
    
    /* 右边应该是 4 / 2 */
    assert(expr->as.binary.right->type == AST_BINARY_DIV);
    
    xr_ast_free(X, ast);
    xray_close(X);
    
    PASS();
}

/* 测试多个表达式 */
static void test_parse_multiple() {
    TEST("多个表达式");
    
    XrayState *X = xray_newstate();
    
    /* 解析多个表达式 */
    AstNode *ast = xr_parse(X, "1 + 2\n3 * 4");
    
    assert(ast != NULL);
    assert(ast->type == AST_PROGRAM);
    assert(ast->as.program.count == 2);
    
    /* 第一个表达式：1 + 2 */
    AstNode *expr1 = ast->as.program.statements[0]->as.expr_stmt;
    assert(expr1->type == AST_BINARY_ADD);
    
    /* 第二个表达式：3 * 4 */
    AstNode *expr2 = ast->as.program.statements[1]->as.expr_stmt;
    assert(expr2->type == AST_BINARY_MUL);
    
    xr_ast_free(X, ast);
    xray_close(X);
    
    PASS();
}

/* ========== AST 打印测试 ========== */

/* 测试 AST 打印 */
static void test_ast_print() {
    TEST("AST 打印");
    
    XrayState *X = xray_newstate();
    AstNode *ast = xr_parse(X, "1 + 2 * 3");
    
    assert(ast != NULL);
    
    printf("\n--- AST 结构 ---\n");
    xr_ast_print(ast, 0);
    printf("--- 结束 ---\n");
    
    xr_ast_free(X, ast);
    xray_close(X);
    
    PASS();
}

/* ========== 主函数 ========== */

int main() {
    printf("=== Xray 语法解析器测试 ===\n\n");
    
    /* 字面量解析测试 */
    test_parse_int();
    test_parse_float();
    test_parse_string();
    test_parse_bool();
    test_parse_null();
    
    /* 算术表达式测试 */
    test_parse_add();
    test_parse_sub();
    test_parse_mul();
    test_parse_div();
    test_parse_mod();
    
    /* 优先级和括号测试 */
    test_parse_precedence();
    test_parse_parentheses();
    
    /* 一元运算符测试 */
    test_parse_negate();
    test_parse_not();
    
    /* 比较和逻辑运算符测试 */
    test_parse_comparisons();
    test_parse_logical();
    
    /* 复杂表达式测试 */
    test_parse_complex();
    test_parse_multiple();
    
    /* AST 打印测试 */
    test_ast_print();
    
    /* 统计 */
    printf("\n=== 测试统计 ===\n");
    printf("通过: %d\n", tests_passed);
    printf("失败: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n✅ 所有测试通过!\n");
        return 0;
    } else {
        printf("\n❌ 有测试失败!\n");
        return 1;
    }
}

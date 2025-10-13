/*
** test_ast.c
** Xray AST 模块测试
** 测试抽象语法树节点的创建和操作
*/

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "../xray.h"
#include "../xast.h"

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

/* ========== 字面量节点测试 ========== */

/* 测试整数字面量 */
static void test_literal_int() {
    TEST("整数字面量");
    
    XrayState *X = xray_newstate();
    AstNode *node = xr_ast_literal_int(X, 42, 1);
    
    assert(node != NULL);
    assert(node->type == AST_LITERAL_INT);
    assert(node->line == 1);
    assert(xr_isint(node->as.literal.value));
    assert(xr_toint(node->as.literal.value) == 42);
    
    xr_ast_free(X, node);
    xray_close(X);
    
    PASS();
}

/* 测试浮点数字面量 */
static void test_literal_float() {
    TEST("浮点数字面量");
    
    XrayState *X = xray_newstate();
    AstNode *node = xr_ast_literal_float(X, 3.14, 1);
    
    assert(node != NULL);
    assert(node->type == AST_LITERAL_FLOAT);
    assert(xr_isfloat(node->as.literal.value));
    assert(xr_tofloat(node->as.literal.value) == 3.14);
    
    xr_ast_free(X, node);
    xray_close(X);
    
    PASS();
}

/* 测试字符串字面量 */
static void test_literal_string() {
    TEST("字符串字面量");
    
    XrayState *X = xray_newstate();
    AstNode *node = xr_ast_literal_string(X, "hello", 1);
    
    assert(node != NULL);
    assert(node->type == AST_LITERAL_STRING);
    assert(xr_isstring(node->as.literal.value));
    
    xr_ast_free(X, node);
    xray_close(X);
    
    PASS();
}

/* 测试 null 字面量 */
static void test_literal_null() {
    TEST("null 字面量");
    
    XrayState *X = xray_newstate();
    AstNode *node = xr_ast_literal_null(X, 1);
    
    assert(node != NULL);
    assert(node->type == AST_LITERAL_NULL);
    assert(xr_isnull(node->as.literal.value));
    
    xr_ast_free(X, node);
    xray_close(X);
    
    PASS();
}

/* 测试布尔字面量 */
static void test_literal_bool() {
    TEST("布尔字面量");
    
    XrayState *X = xray_newstate();
    
    /* 测试 true */
    AstNode *node_true = xr_ast_literal_bool(X, 1, 1);
    assert(node_true != NULL);
    assert(node_true->type == AST_LITERAL_TRUE);
    assert(xr_isbool(node_true->as.literal.value));
    assert(xr_tobool(node_true->as.literal.value) == 1);
    
    /* 测试 false */
    AstNode *node_false = xr_ast_literal_bool(X, 0, 1);
    assert(node_false != NULL);
    assert(node_false->type == AST_LITERAL_FALSE);
    assert(xr_isbool(node_false->as.literal.value));
    assert(xr_tobool(node_false->as.literal.value) == 0);
    
    xr_ast_free(X, node_true);
    xr_ast_free(X, node_false);
    xray_close(X);
    
    PASS();
}

/* ========== 运算符节点测试 ========== */

/* 测试二元运算节点 */
static void test_binary_node() {
    TEST("二元运算节点");
    
    XrayState *X = xray_newstate();
    
    /* 创建 1 + 2 */
    AstNode *left = xr_ast_literal_int(X, 1, 1);
    AstNode *right = xr_ast_literal_int(X, 2, 1);
    AstNode *add = xr_ast_binary(X, AST_BINARY_ADD, left, right, 1);
    
    assert(add != NULL);
    assert(add->type == AST_BINARY_ADD);
    assert(add->as.binary.left == left);
    assert(add->as.binary.right == right);
    
    xr_ast_free(X, add);
    xray_close(X);
    
    PASS();
}

/* 测试一元运算节点 */
static void test_unary_node() {
    TEST("一元运算节点");
    
    XrayState *X = xray_newstate();
    
    /* 创建 -5 */
    AstNode *operand = xr_ast_literal_int(X, 5, 1);
    AstNode *negate = xr_ast_unary(X, AST_UNARY_NEG, operand, 1);
    
    assert(negate != NULL);
    assert(negate->type == AST_UNARY_NEG);
    assert(negate->as.unary.operand == operand);
    
    xr_ast_free(X, negate);
    xray_close(X);
    
    PASS();
}

/* ========== 复杂表达式测试 ========== */

/* 测试嵌套表达式 */
static void test_nested_expr() {
    TEST("嵌套表达式");
    
    XrayState *X = xray_newstate();
    
    /* 创建 (1 + 2) * 3 */
    AstNode *one = xr_ast_literal_int(X, 1, 1);
    AstNode *two = xr_ast_literal_int(X, 2, 1);
    AstNode *three = xr_ast_literal_int(X, 3, 1);
    
    AstNode *add = xr_ast_binary(X, AST_BINARY_ADD, one, two, 1);
    AstNode *grouping = xr_ast_grouping(X, add, 1);
    AstNode *mul = xr_ast_binary(X, AST_BINARY_MUL, grouping, three, 1);
    
    assert(mul != NULL);
    assert(mul->type == AST_BINARY_MUL);
    assert(mul->as.binary.left->type == AST_GROUPING);
    
    xr_ast_free(X, mul);
    xray_close(X);
    
    PASS();
}

/* ========== 程序节点测试 ========== */

/* 测试程序节点 */
static void test_program_node() {
    TEST("程序节点");
    
    XrayState *X = xray_newstate();
    
    /* 创建程序 */
    AstNode *program = xr_ast_program(X);
    assert(program != NULL);
    assert(program->type == AST_PROGRAM);
    assert(program->as.program.count == 0);
    
    /* 添加语句 */
    AstNode *expr1 = xr_ast_literal_int(X, 1, 1);
    AstNode *stmt1 = xr_ast_expr_stmt(X, expr1, 1);
    xr_ast_program_add(X, program, stmt1);
    
    AstNode *expr2 = xr_ast_literal_int(X, 2, 2);
    AstNode *stmt2 = xr_ast_expr_stmt(X, expr2, 2);
    xr_ast_program_add(X, program, stmt2);
    
    assert(program->as.program.count == 2);
    assert(program->as.program.statements[0] == stmt1);
    assert(program->as.program.statements[1] == stmt2);
    
    xr_ast_free(X, program);
    xray_close(X);
    
    PASS();
}

/* ========== AST 打印测试 ========== */

/* 测试 AST 打印 */
static void test_ast_print() {
    TEST("AST 打印");
    
    XrayState *X = xray_newstate();
    
    /* 创建 1 + 2 */
    AstNode *left = xr_ast_literal_int(X, 1, 1);
    AstNode *right = xr_ast_literal_int(X, 2, 1);
    AstNode *add = xr_ast_binary(X, AST_BINARY_ADD, left, right, 1);
    
    printf("\n");
    xr_ast_print(add, 0);
    
    xr_ast_free(X, add);
    xray_close(X);
    
    PASS();
}

/* ========== 主函数 ========== */

int main() {
    printf("=== Xray AST 模块测试 ===\n\n");
    
    /* 字面量测试 */
    test_literal_int();
    test_literal_float();
    test_literal_string();
    test_literal_null();
    test_literal_bool();
    
    /* 运算符测试 */
    test_binary_node();
    test_unary_node();
    
    /* 复杂表达式测试 */
    test_nested_expr();
    
    /* 程序节点测试 */
    test_program_node();
    
    /* 打印测试 */
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


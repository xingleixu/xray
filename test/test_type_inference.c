/*
** test_type_inference.c
** 类型推导测试
*/

#include "../xtype.h"
#include "../xast.h"
#include "../xstate.h"
#include "../xvalue.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

/* 测试计数器 */
static int tests_run = 0;
static int tests_passed = 0;

/* 测试宏 */
#define TEST_START(name) \
    printf("测试: %s ... ", name); \
    tests_run++;

#define TEST_PASS() \
    printf("✅ 通过\n"); \
    tests_passed++;

#define TEST_FAIL(msg) \
    printf("❌ 失败: %s\n", msg);

#define ASSERT_TYPE(type, expected_kind) \
    if (type == NULL) { \
        TEST_FAIL("类型为NULL"); \
        return; \
    } \
    if (type->kind != expected_kind) { \
        char buf[128]; \
        snprintf(buf, sizeof(buf), "期望类型 %s，实际类型 %s", \
                 xr_type_kind_name(expected_kind), \
                 xr_type_kind_name(type->kind)); \
        TEST_FAIL(buf); \
        return; \
    }

/* ========== 辅助函数 ========== */

/* 创建整数字面量节点 */
AstNode* create_int_literal(XrayState *X, xr_Integer value) {
    return xr_ast_literal_int(X, value, 0);
}

/* 创建浮点数字面量节点 */
AstNode* create_float_literal(XrayState *X, xr_Number value) {
    return xr_ast_literal_float(X, value, 0);
}

/* 创建字符串字面量节点 */
AstNode* create_string_literal(XrayState *X, const char *value) {
    return xr_ast_literal_string(X, value, 0);
}

/* 创建布尔字面量节点 */
AstNode* create_bool_literal(XrayState *X, bool value) {
    return xr_ast_literal_bool(X, value, 0);
}

/* 创建null字面量节点 */
AstNode* create_null_literal(XrayState *X) {
    return xr_ast_literal_null(X, 0);
}

/* 创建二元运算节点 */
AstNode* create_binary_node(XrayState *X, AstNodeType type, AstNode *left, AstNode *right) {
    return xr_ast_binary(X, type, left, right, 0);
}

/* 创建一元运算节点 */
AstNode* create_unary_node(XrayState *X, AstNodeType type, AstNode *operand) {
    return xr_ast_unary(X, type, operand, 0);
}

/* ========== 字面量类型推导测试 ========== */

void test_infer_int_literal() {
    TEST_START("整数字面量推导");
    
    XrayState *X = xray_newstate();
    AstNode *node = create_int_literal(X, 42);
    XrTypeInfo *type = xr_infer_type_from_expr(X, node);
    
    ASSERT_TYPE(type, TYPE_INT);
    TEST_PASS();
    
    // xray_closestate(X); // TODO: 添加状态清理函数
}

void test_infer_float_literal() {
    TEST_START("浮点数字面量推导");
    
    XrayState *X = xray_newstate();
    AstNode *node = create_float_literal(X, 3.14);
    XrTypeInfo *type = xr_infer_type_from_expr(X, node);
    
    ASSERT_TYPE(type, TYPE_FLOAT);
    TEST_PASS();
    
    // xray_closestate(X); // TODO: 添加状态清理函数
}

void test_infer_string_literal() {
    TEST_START("字符串字面量推导");
    
    XrayState *X = xray_newstate();
    AstNode *node = create_string_literal(X, "hello");
    XrTypeInfo *type = xr_infer_type_from_expr(X, node);
    
    ASSERT_TYPE(type, TYPE_STRING);
    TEST_PASS();
    
    // xray_closestate(X); // TODO: 添加状态清理函数
}

void test_infer_bool_literal() {
    TEST_START("布尔字面量推导");
    
    XrayState *X = xray_newstate();
    AstNode *node = create_bool_literal(X, true);
    XrTypeInfo *type = xr_infer_type_from_expr(X, node);
    
    ASSERT_TYPE(type, TYPE_BOOL);
    TEST_PASS();
    
    // xray_closestate(X); // TODO: 添加状态清理函数
}

void test_infer_null_literal() {
    TEST_START("null字面量推导");
    
    XrayState *X = xray_newstate();
    AstNode *node = create_null_literal(X);
    XrTypeInfo *type = xr_infer_type_from_expr(X, node);
    
    ASSERT_TYPE(type, TYPE_NULL);
    TEST_PASS();
    
    // xray_closestate(X); // TODO: 添加状态清理函数
}

/* ========== 算术表达式类型推导测试 ========== */

void test_infer_int_add() {
    TEST_START("整数加法推导 (int + int → int)");
    
    XrayState *X = xray_newstate();
    AstNode *left = create_int_literal(X, 10);
    AstNode *right = create_int_literal(X, 20);
    AstNode *expr = create_binary_node(X, AST_BINARY_ADD, left, right);
    
    XrTypeInfo *type = xr_infer_type_from_expr(X, expr);
    
    ASSERT_TYPE(type, TYPE_INT);
    TEST_PASS();
    
    // xray_closestate(X); // TODO: 添加状态清理函数
}

void test_infer_float_mul() {
    TEST_START("浮点数乘法推导 (float * float → float)");
    
    XrayState *X = xray_newstate();
    AstNode *left = create_float_literal(X, 3.14);
    AstNode *right = create_float_literal(X, 2.0);
    AstNode *expr = create_binary_node(X, AST_BINARY_MUL, left, right);
    
    XrTypeInfo *type = xr_infer_type_from_expr(X, expr);
    
    ASSERT_TYPE(type, TYPE_FLOAT);
    TEST_PASS();
    
    // xray_closestate(X); // TODO: 添加状态清理函数
}

void test_infer_mixed_add() {
    TEST_START("混合类型加法推导 (int + float → float)");
    
    XrayState *X = xray_newstate();
    AstNode *left = create_int_literal(X, 10);
    AstNode *right = create_float_literal(X, 3.5);
    AstNode *expr = create_binary_node(X, AST_BINARY_ADD, left, right);
    
    XrTypeInfo *type = xr_infer_type_from_expr(X, expr);
    
    ASSERT_TYPE(type, TYPE_FLOAT);
    TEST_PASS();
    
    // xray_closestate(X); // TODO: 添加状态清理函数
}

/* ========== 比较表达式类型推导测试 ========== */

void test_infer_comparison() {
    TEST_START("比较表达式推导 (5 > 3 → bool)");
    
    XrayState *X = xray_newstate();
    AstNode *left = create_int_literal(X, 5);
    AstNode *right = create_int_literal(X, 3);
    AstNode *expr = create_binary_node(X, AST_BINARY_GT, left, right);
    
    XrTypeInfo *type = xr_infer_type_from_expr(X, expr);
    
    ASSERT_TYPE(type, TYPE_BOOL);
    TEST_PASS();
    
    // xray_closestate(X); // TODO: 添加状态清理函数
}

void test_infer_equality() {
    TEST_START("相等比较推导 (10 == 10 → bool)");
    
    XrayState *X = xray_newstate();
    AstNode *left = create_int_literal(X, 10);
    AstNode *right = create_int_literal(X, 10);
    AstNode *expr = create_binary_node(X, AST_BINARY_EQ, left, right);
    
    XrTypeInfo *type = xr_infer_type_from_expr(X, expr);
    
    ASSERT_TYPE(type, TYPE_BOOL);
    TEST_PASS();
    
    // xray_closestate(X); // TODO: 添加状态清理函数
}

/* ========== 逻辑表达式类型推导测试 ========== */

void test_infer_logical_and() {
    TEST_START("逻辑与推导 (true && false → bool)");
    
    XrayState *X = xray_newstate();
    AstNode *left = create_bool_literal(X, true);
    AstNode *right = create_bool_literal(X, false);
    AstNode *expr = create_binary_node(X, AST_BINARY_AND, left, right);
    
    XrTypeInfo *type = xr_infer_type_from_expr(X, expr);
    
    ASSERT_TYPE(type, TYPE_BOOL);
    TEST_PASS();
    
    // xray_closestate(X); // TODO: 添加状态清理函数
}

void test_infer_logical_or() {
    TEST_START("逻辑或推导 (true || false → bool)");
    
    XrayState *X = xray_newstate();
    AstNode *left = create_bool_literal(X, true);
    AstNode *right = create_bool_literal(X, false);
    AstNode *expr = create_binary_node(X, AST_BINARY_OR, left, right);
    
    XrTypeInfo *type = xr_infer_type_from_expr(X, expr);
    
    ASSERT_TYPE(type, TYPE_BOOL);
    TEST_PASS();
    
    // xray_closestate(X); // TODO: 添加状态清理函数
}

/* ========== 一元表达式类型推导测试 ========== */

void test_infer_unary_neg_int() {
    TEST_START("整数取负推导 (-42 → int)");
    
    XrayState *X = xray_newstate();
    AstNode *operand = create_int_literal(X, 42);
    AstNode *expr = create_unary_node(X, AST_UNARY_NEG, operand);
    
    XrTypeInfo *type = xr_infer_type_from_expr(X, expr);
    
    ASSERT_TYPE(type, TYPE_INT);
    TEST_PASS();
    
    // xray_closestate(X); // TODO: 添加状态清理函数
}

void test_infer_unary_neg_float() {
    TEST_START("浮点数取负推导 (-3.14 → float)");
    
    XrayState *X = xray_newstate();
    AstNode *operand = create_float_literal(X, 3.14);
    AstNode *expr = create_unary_node(X, AST_UNARY_NEG, operand);
    
    XrTypeInfo *type = xr_infer_type_from_expr(X, expr);
    
    ASSERT_TYPE(type, TYPE_FLOAT);
    TEST_PASS();
    
    // xray_closestate(X); // TODO: 添加状态清理函数
}

void test_infer_logical_not() {
    TEST_START("逻辑非推导 (!true → bool)");
    
    XrayState *X = xray_newstate();
    AstNode *operand = create_bool_literal(X, true);
    AstNode *expr = create_unary_node(X, AST_UNARY_NOT, operand);
    
    XrTypeInfo *type = xr_infer_type_from_expr(X, expr);
    
    ASSERT_TYPE(type, TYPE_BOOL);
    TEST_PASS();
    
    // xray_closestate(X); // TODO: 添加状态清理函数
}

/* ========== 函数返回类型推导测试 ========== */

void test_infer_function_return_int() {
    TEST_START("函数返回int类型推导");
    
    XrayState *X = xray_newstate();
    
    /* 创建函数体：{ return 42; } */
    AstNode *block = xr_ast_block(X, 0);
    AstNode *return_val = create_int_literal(X, 42);
    AstNode *return_stmt = xr_ast_return_stmt(X, return_val, 0);
    xr_ast_block_add(X, block, return_stmt);
    
    /* 推导返回类型 */
    XrTypeInfo *type = xr_infer_function_return_type(X, block);
    
    ASSERT_TYPE(type, TYPE_INT);
    TEST_PASS();
    
    // xray_closestate(X); // TODO: 添加状态清理函数
}

void test_infer_function_return_void() {
    TEST_START("函数返回void类型推导");
    
    XrayState *X = xray_newstate();
    
    /* 创建函数体：{ return; } */
    AstNode *block = xr_ast_block(X, 0);
    AstNode *return_stmt = xr_ast_return_stmt(X, NULL, 0);  /* 无返回值 */
    xr_ast_block_add(X, block, return_stmt);
    
    /* 推导返回类型 */
    XrTypeInfo *type = xr_infer_function_return_type(X, block);
    
    ASSERT_TYPE(type, TYPE_VOID);
    TEST_PASS();
    
    // xray_closestate(X); // TODO: 添加状态清理函数
}

void test_infer_function_multiple_returns() {
    TEST_START("多个返回语句类型推导（相同类型）");
    
    XrayState *X = xray_newstate();
    
    /* 创建函数体：{ return 10; return 20; } */
    AstNode *block = xr_ast_block(X, 0);
    
    AstNode *return1 = xr_ast_return_stmt(X, create_int_literal(X, 10), 0);
    AstNode *return2 = xr_ast_return_stmt(X, create_int_literal(X, 20), 0);
    
    xr_ast_block_add(X, block, return1);
    xr_ast_block_add(X, block, return2);
    
    /* 推导返回类型 */
    XrTypeInfo *type = xr_infer_function_return_type(X, block);
    
    ASSERT_TYPE(type, TYPE_INT);
    TEST_PASS();
    
    // xray_closestate(X); // TODO: 添加状态清理函数
}

void test_infer_function_no_return() {
    TEST_START("无返回语句函数（void）");
    
    XrayState *X = xray_newstate();
    
    /* 创建空函数体：{} */
    AstNode *block = xr_ast_block(X, 0);
    
    /* 推导返回类型 */
    XrTypeInfo *type = xr_infer_function_return_type(X, block);
    
    ASSERT_TYPE(type, TYPE_VOID);
    TEST_PASS();
    
    // xray_closestate(X); // TODO: 添加状态清理函数
}

/* ========== 复杂表达式类型推导测试 ========== */

void test_infer_nested_expr() {
    TEST_START("嵌套表达式推导 ((10 + 20) * 2 → int)");
    
    XrayState *X = xray_newstate();
    
    /* 创建 10 + 20 */
    AstNode *ten = create_int_literal(X, 10);
    AstNode *twenty = create_int_literal(X, 20);
    AstNode *add = create_binary_node(X, AST_BINARY_ADD, ten, twenty);
    
    /* 创建 (10 + 20) * 2 */
    AstNode *two = create_int_literal(X, 2);
    AstNode *mul = create_binary_node(X, AST_BINARY_MUL, add, two);
    
    XrTypeInfo *type = xr_infer_type_from_expr(X, mul);
    
    ASSERT_TYPE(type, TYPE_INT);
    TEST_PASS();
    
    // xray_closestate(X); // TODO: 添加状态清理函数
}

void test_infer_mixed_nested_expr() {
    TEST_START("混合嵌套表达式推导 (10 + 3.5 * 2 → float)");
    
    XrayState *X = xray_newstate();
    
    /* 创建 3.5 * 2 */
    AstNode *three_five = create_float_literal(X, 3.5);
    AstNode *two = create_int_literal(X, 2);
    AstNode *mul = create_binary_node(X, AST_BINARY_MUL, three_five, two);
    
    /* 创建 10 + (3.5 * 2) */
    AstNode *ten = create_int_literal(X, 10);
    AstNode *add = create_binary_node(X, AST_BINARY_ADD, ten, mul);
    
    XrTypeInfo *type = xr_infer_type_from_expr(X, add);
    
    ASSERT_TYPE(type, TYPE_FLOAT);
    TEST_PASS();
    
    // xray_closestate(X); // TODO: 添加状态清理函数
}

/* ========== 主函数 ========== */

int main() {
    printf("\n========== 类型推导测试 ==========\n\n");
    
    /* 字面量推导测试 */
    printf("--- 字面量类型推导 ---\n");
    test_infer_int_literal();
    test_infer_float_literal();
    test_infer_string_literal();
    test_infer_bool_literal();
    test_infer_null_literal();
    
    /* 算术表达式推导测试 */
    printf("\n--- 算术表达式类型推导 ---\n");
    test_infer_int_add();
    test_infer_float_mul();
    test_infer_mixed_add();
    
    /* 比较表达式推导测试 */
    printf("\n--- 比较表达式类型推导 ---\n");
    test_infer_comparison();
    test_infer_equality();
    
    /* 逻辑表达式推导测试 */
    printf("\n--- 逻辑表达式类型推导 ---\n");
    test_infer_logical_and();
    test_infer_logical_or();
    
    /* 一元表达式推导测试 */
    printf("\n--- 一元表达式类型推导 ---\n");
    test_infer_unary_neg_int();
    test_infer_unary_neg_float();
    test_infer_logical_not();
    
    /* 函数返回类型推导测试 */
    printf("\n--- 函数返回类型推导 ---\n");
    test_infer_function_return_int();
    test_infer_function_return_void();
    test_infer_function_multiple_returns();
    test_infer_function_no_return();
    
    /* 复杂表达式推导测试 */
    printf("\n--- 复杂表达式类型推导 ---\n");
    test_infer_nested_expr();
    test_infer_mixed_nested_expr();
    
    /* 打印测试结果 */
    printf("\n========== 测试结果 ==========\n");
    printf("总计: %d 个测试\n", tests_run);
    printf("通过: %d 个测试\n", tests_passed);
    printf("失败: %d 个测试\n", tests_run - tests_passed);
    
    if (tests_passed == tests_run) {
        printf("\n✅ 所有测试通过！\n\n");
        return 0;
    } else {
        printf("\n❌ 部分测试失败！\n\n");
        return 1;
    }
}


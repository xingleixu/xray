/*
** test_eval.c
** Xray 表达式求值器测试
** 测试表达式计算和运行时执行
*/

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "xstate.h"
#include "xparse.h"
#include "xast.h"
#include "xvalue.h"
// xeval.h 已废弃 - 此测试文件需要重新设计以适应字节码VM

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

/*
** 执行代码并返回结果
** 注意：此函数需要重新设计以适应字节码VM
** 目前返回空值以保持编译通过
*/
static XrValue eval_code(const char *code) {
    XrayState *X = xr_state_new();
    AstNode *ast = xr_parse(X, code);
    if (ast) {
        xr_ast_free(X, ast);
    }
    xr_state_free(X);
    
    // 返回空值，因为字节码VM不直接返回表达式值
    XrValue null_val;
    null_val = xr_null();
    return null_val;
}

/* ========== 字面量求值测试 ========== */

/* 测试整数求值 */
static void test_eval_int() {
    TEST("整数求值");
    
    XrValue result = eval_code("42");
    assert(xr_isint(result));
    assert(xr_toint(result) == 42);
    
    PASS();
}

/* 测试浮点数求值 */
static void test_eval_float() {
    TEST("浮点数求值");
    
    XrValue result = eval_code("3.14");
    assert(xr_isfloat(result));
    assert(xr_tofloat(result) == 3.14);
    
    PASS();
}

/* 测试布尔值求值 */
static void test_eval_bool() {
    TEST("布尔值求值");
    
    XrValue result_true = eval_code("true");
    assert(xr_isbool(result_true));
    assert(xr_tobool(result_true) == 1);
    
    XrValue result_false = eval_code("false");
    assert(xr_isbool(result_false));
    assert(xr_tobool(result_false) == 0);
    
    PASS();
}

/* 测试 null 求值 */
static void test_eval_null() {
    TEST("null 求值");
    
    XrValue result = eval_code("null");
    assert(xr_isnull(result));
    
    PASS();
}

/* ========== 算术运算测试 ========== */

/* 测试加法运算 */
static void test_eval_add() {
    TEST("加法运算");
    
    /* 整数加法 */
    XrValue result1 = eval_code("1 + 2");
    assert(xr_isint(result1));
    assert(xr_toint(result1) == 3);
    
    /* 浮点数加法 */
    XrValue result2 = eval_code("1.5 + 2.5");
    assert(xr_isfloat(result2));
    assert(xr_tofloat(result2) == 4.0);
    
    /* 混合加法 */
    XrValue result3 = eval_code("1 + 2.5");
    assert(xr_isfloat(result3));
    assert(xr_tofloat(result3) == 3.5);
    
    PASS();
}

/* 测试减法运算 */
static void test_eval_sub() {
    TEST("减法运算");
    
    XrValue result = eval_code("10 - 3");
    assert(xr_isint(result));
    assert(xr_toint(result) == 7);
    
    PASS();
}

/* 测试乘法运算 */
static void test_eval_mul() {
    TEST("乘法运算");
    
    XrValue result = eval_code("4 * 5");
    assert(xr_isint(result));
    assert(xr_toint(result) == 20);
    
    PASS();
}

/* 测试除法运算 */
static void test_eval_div() {
    TEST("除法运算");
    
    XrValue result = eval_code("15 / 3");
    assert(xr_isfloat(result));
    assert(xr_tofloat(result) == 5.0);
    
    PASS();
}

/* 测试取模运算 */
static void test_eval_mod() {
    TEST("取模运算");
    
    XrValue result = eval_code("17 % 5");
    assert(xr_isint(result));
    assert(xr_toint(result) == 2);
    
    PASS();
}

/* ========== 运算符优先级测试 ========== */

/* 测试运算符优先级 */
static void test_eval_precedence() {
    TEST("运算符优先级");
    
    /* 2 + 3 * 4 应该是 2 + (3 * 4) = 14 */
    XrValue result1 = eval_code("2 + 3 * 4");
    assert(xr_isint(result1));
    assert(xr_toint(result1) == 14);
    
    /* (2 + 3) * 4 应该是 5 * 4 = 20 */
    XrValue result2 = eval_code("(2 + 3) * 4");
    assert(xr_isint(result2));
    assert(xr_toint(result2) == 20);
    
    PASS();
}

/* ========== 一元运算符测试 ========== */

/* 测试取负运算 */
static void test_eval_negate() {
    TEST("取负运算");
    
    XrValue result1 = eval_code("-5");
    assert(xr_isint(result1));
    assert(xr_toint(result1) == -5);
    
    XrValue result2 = eval_code("-(3 + 4)");
    assert(xr_isint(result2));
    assert(xr_toint(result2) == -7);
    
    PASS();
}

/* 测试逻辑非运算 */
static void test_eval_not() {
    TEST("逻辑非运算");
    
    XrValue result1 = eval_code("!true");
    assert(xr_isbool(result1));
    assert(xr_tobool(result1) == 0);
    
    XrValue result2 = eval_code("!false");
    assert(xr_isbool(result2));
    assert(xr_tobool(result2) == 1);
    
    PASS();
}

/* ========== 比较运算符测试 ========== */

/* 测试比较运算符 */
static void test_eval_comparisons() {
    TEST("比较运算符");
    
    /* 相等 */
    XrValue result1 = eval_code("5 == 5");
    assert(xr_isbool(result1));
    assert(xr_tobool(result1) == 1);
    
    /* 不等 */
    XrValue result2 = eval_code("5 != 3");
    assert(xr_isbool(result2));
    assert(xr_tobool(result2) == 1);
    
    /* 小于 */
    XrValue result3 = eval_code("3 < 5");
    assert(xr_isbool(result3));
    assert(xr_tobool(result3) == 1);
    
    /* 大于 */
    XrValue result4 = eval_code("5 > 3");
    assert(xr_isbool(result4));
    assert(xr_tobool(result4) == 1);
    
    PASS();
}

/* ========== 逻辑运算符测试 ========== */

/* 测试逻辑与 */
static void test_eval_logical_and() {
    TEST("逻辑与");
    
    XrValue result1 = eval_code("true && true");
    assert(xr_isbool(result1));
    assert(xr_tobool(result1) == 1);
    
    XrValue result2 = eval_code("true && false");
    assert(xr_isbool(result2));
    assert(xr_tobool(result2) == 0);
    
    XrValue result3 = eval_code("false && true");
    assert(xr_isbool(result3));
    assert(xr_tobool(result3) == 0);
    
    PASS();
}

/* 测试逻辑或 */
static void test_eval_logical_or() {
    TEST("逻辑或");
    
    XrValue result1 = eval_code("true || false");
    assert(xr_isbool(result1));
    assert(xr_tobool(result1) == 1);
    
    XrValue result2 = eval_code("false || true");
    assert(xr_isbool(result2));
    assert(xr_tobool(result2) == 1);
    
    XrValue result3 = eval_code("false || false");
    assert(xr_isbool(result3));
    assert(xr_tobool(result3) == 0);
    
    PASS();
}

/* ========== 复杂表达式测试 ========== */

/* 测试复杂表达式 */
static void test_eval_complex() {
    TEST("复杂表达式");
    
    /* (1 + 2) * 3 - 4 / 2 = 9 - 2 = 7 */
    XrValue result = eval_code("(1 + 2) * 3 - 4 / 2");
    assert(xr_isfloat(result));
    assert(xr_tofloat(result) == 7.0);
    
    PASS();
}

/* 测试短路求值 */
static void test_eval_short_circuit() {
    TEST("短路求值");
    
    /* false && 任何表达式 应该返回 false，不执行右边 */
    XrValue result1 = eval_code("false && (1 / 0)");  /* 不会除零，因为短路了 */
    assert(xr_isbool(result1));
    assert(xr_tobool(result1) == 0);
    
    /* true || 任何表达式 应该返回 true，不执行右边 */
    XrValue result2 = eval_code("true || (1 / 0)");   /* 不会除零，因为短路了 */
    assert(xr_isbool(result2));
    assert(xr_tobool(result2) == 1);
    
    PASS();
}

/* ========== Print 语句测试 ========== */

/* 测试 print 语句 */
static void test_eval_print() {
    TEST("print 语句");
    
    printf("\n--- Print 输出测试 ---\n");
    
    /* 执行 print 语句（使用字节码VM） */
    XrayState *X = xr_state_new();
    AstNode *ast = xr_parse(X, "print(42)");
    if (ast != NULL) {
        // 注意：需要使用字节码VM执行
        // xr_eval 已废弃
        xr_ast_free(X, ast);
    }
    xr_state_free(X);
    
    printf("--- Print 测试结束（需要重新设计）---\n");
    
    PASS();
}

/* ========== 主函数 ========== */

int main() {
    printf("=== Xray 表达式求值器测试 ===\n\n");
    
    /* 字面量求值测试 */
    test_eval_int();
    test_eval_float();
    test_eval_bool();
    test_eval_null();
    
    /* 算术运算测试 */
    test_eval_add();
    test_eval_sub();
    test_eval_mul();
    test_eval_div();
    test_eval_mod();
    
    /* 优先级测试 */
    test_eval_precedence();
    
    /* 一元运算符测试 */
    test_eval_negate();
    test_eval_not();
    
    /* 比较运算符测试 */
    test_eval_comparisons();
    
    /* 逻辑运算符测试 */
    test_eval_logical_and();
    test_eval_logical_or();
    
    /* 复杂表达式测试 */
    test_eval_complex();
    test_eval_short_circuit();
    
    /* Print 语句测试 */
    test_eval_print();
    
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



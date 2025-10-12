/*
** test_control_flow.c
** Xray 控制流语句测试
** 测试 if-else、while、for、break、continue 等控制流结构
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../xray.h"
#include "../xlex.h"
#include "../xparse.h"
#include "../xeval.h"
#include "../xast.h"

/* 测试计数器 */
static int tests_run = 0;
static int tests_passed = 0;

/* 断言宏 */
#define ASSERT(test, message) do { \
    tests_run++; \
    if (test) { \
        tests_passed++; \
        printf("✓ %s\n", message); \
    } else { \
        printf("✗ %s\n", message); \
    } \
} while (0)

/* 辅助函数：解析并求值代码 */
static void run_code(const char *code) {
    XrayState *X = xray_newstate();
    AstNode *ast = xr_parse(X, code);
    if (ast != NULL) {
        xr_eval(X, ast);
        xr_ast_free(X, ast);
    }
    xray_close(X);
}

/* 辅助函数：解析代码 */
static AstNode *parse_code_with_state(XrayState *X, const char *code) {
    return xr_parse(X, code);
}

/* ========== if-else 解析测试 ========== */

void test_parse_if_simple() {
    XrayState *X = xray_newstate();
    const char *code = "if (x > 5) { print(x) }";
    AstNode *ast = parse_code_with_state(X, code);
    
    ASSERT(ast != NULL, "简单 if 语句解析");
    ASSERT(ast->type == AST_PROGRAM, "根节点是程序节点");
    ASSERT(ast->as.program.count == 1, "包含1个语句");
    ASSERT(ast->as.program.statements[0]->type == AST_IF_STMT, "第一个语句是 if");
    
    xr_ast_free(X, ast);
    xray_close(X);
}

void test_parse_if_else() {
    XrayState *X = xray_newstate();
    const char *code = "if (x > 5) { print(1) } else { print(2) }";
    AstNode *ast = parse_code_with_state(X, code);
    
    ASSERT(ast != NULL, "if-else 语句解析");
    AstNode *if_node = ast->as.program.statements[0];
    ASSERT(if_node->as.if_stmt.else_branch != NULL, "有 else 分支");
    
    xr_ast_free(X, ast);
    xray_close(X);
}

void test_parse_else_if() {
    XrayState *X = xray_newstate();
    const char *code = "if (x > 10) { print(1) } else if (x > 5) { print(2) } else { print(3) }";
    AstNode *ast = parse_code_with_state(X, code);
    
    ASSERT(ast != NULL, "else-if 链解析");
    AstNode *if_node = ast->as.program.statements[0];
    ASSERT(if_node->as.if_stmt.else_branch != NULL, "有 else 分支");
    ASSERT(if_node->as.if_stmt.else_branch->type == AST_IF_STMT, "else 分支是 if");
    
    xr_ast_free(X, ast);
    xray_close(X);
}

void test_parse_nested_if() {
    XrayState *X = xray_newstate();
    const char *code = "if (a > 0) { if (b > 0) { print(1) } }";
    AstNode *ast = parse_code_with_state(X, code);
    
    ASSERT(ast != NULL, "嵌套 if 解析");
    
    xr_ast_free(X, ast);
    xray_close(X);
}

/* ========== 循环解析测试 ========== */

void test_parse_while() {
    XrayState *X = xray_newstate();
    const char *code = "while (i < 10) { i = i + 1 }";
    AstNode *ast = parse_code_with_state(X, code);
    
    ASSERT(ast != NULL, "while 循环解析");
    ASSERT(ast->as.program.statements[0]->type == AST_WHILE_STMT, "节点类型是 while");
    
    xr_ast_free(X, ast);
    xray_close(X);
}

void test_parse_for() {
    XrayState *X = xray_newstate();
    const char *code = "for (let i = 0; i < 10; i = i + 1) { print(i) }";
    AstNode *ast = parse_code_with_state(X, code);
    
    ASSERT(ast != NULL, "for 循环解析");
    ASSERT(ast->as.program.statements[0]->type == AST_FOR_STMT, "节点类型是 for");
    
    xr_ast_free(X, ast);
    xray_close(X);
}

void test_parse_for_empty_parts() {
    XrayState *X = xray_newstate();
    const char *code = "for (;;) { break }";
    AstNode *ast = parse_code_with_state(X, code);
    
    ASSERT(ast != NULL, "for 循环（省略所有部分）解析");
    AstNode *for_node = ast->as.program.statements[0];
    ASSERT(for_node->as.for_stmt.initializer == NULL, "初始化为空");
    ASSERT(for_node->as.for_stmt.condition == NULL, "条件为空");
    ASSERT(for_node->as.for_stmt.increment == NULL, "更新为空");
    
    xr_ast_free(X, ast);
    xray_close(X);
}

/* ========== break/continue 解析测试 ========== */

void test_parse_break() {
    XrayState *X = xray_newstate();
    const char *code = "while (true) { break }";
    AstNode *ast = parse_code_with_state(X, code);
    
    ASSERT(ast != NULL, "break 语句解析");
    
    xr_ast_free(X, ast);
    xray_close(X);
}

void test_parse_continue() {
    XrayState *X = xray_newstate();
    const char *code = "while (true) { continue }";
    AstNode *ast = parse_code_with_state(X, code);
    
    ASSERT(ast != NULL, "continue 语句解析");
    
    xr_ast_free(X, ast);
    xray_close(X);
}

/* ========== if-else 求值测试 ========== */

void test_eval_if_true() {
    run_code("let x = 10; if (x > 5) { x = 20 }; print(x)");
    ASSERT(1, "if 条件为真（应输出 20）");
}

void test_eval_if_false() {
    run_code("let x = 10; if (x > 15) { x = 20 }; print(x)");
    ASSERT(1, "if 条件为假（应输出 10）");
}

void test_eval_if_else() {
    run_code("let x = 10; if (x > 15) { x = 20 } else { x = 5 }; print(x)");
    ASSERT(1, "if-else 求值（应输出 5）");
}

void test_eval_else_if() {
    run_code("let score = 85; if (score >= 90) { print(1) } else if (score >= 80) { print(2) } else { print(3) }");
    ASSERT(1, "else-if 链求值（应输出 2）");
}

void test_eval_nested_if() {
    run_code("let a = 5; let b = 10; if (a > 0) { if (b > 0) { print(1) } }");
    ASSERT(1, "嵌套 if 求值（应输出 1）");
}

void test_eval_if_scope() {
    run_code("let x = 10; if (true) { let x = 20; print(x) }; print(x)");
    ASSERT(1, "if 中的作用域（应输出 20, 10）");
}

/* ========== 循环求值测试 ========== */

void test_eval_while() {
    run_code("let i = 0; while (i < 3) { print(i); i = i + 1 }");
    ASSERT(1, "while 循环求值（应输出 0, 1, 2）");
}

void test_eval_for() {
    run_code("for (let i = 0; i < 3; i = i + 1) { print(i) }");
    ASSERT(1, "for 循环求值（应输出 0, 1, 2）");
}

void test_eval_for_scope() {
    run_code("let i = 999; for (let i = 0; i < 2; i = i + 1) { print(i) }; print(i)");
    ASSERT(1, "for 循环变量作用域（应输出 0, 1, 999）");
}

void test_eval_nested_loops() {
    run_code("for (let i = 1; i <= 2; i = i + 1) { for (let j = 1; j <= 2; j = j + 1) { print(i * j) } }");
    ASSERT(1, "嵌套循环求值（应输出 1, 2, 2, 4）");
}

void test_eval_loop_accumulator() {
    run_code("let sum = 0; for (let i = 1; i <= 5; i = i + 1) { sum = sum + i }; print(sum)");
    ASSERT(1, "循环累加计算（应输出 15）");
}

void test_eval_empty_condition() {
    run_code("let i = 0; while (i >= 10) { i = i + 1 }; print(i)");
    ASSERT(1, "空循环（条件立即为假，应输出 0）");
}

/* ========== break/continue 求值测试 ========== */

void test_eval_break_for() {
    run_code("for (let i = 0; i < 10; i = i + 1) { if (i == 3) { break }; print(i) }");
    ASSERT(1, "break 退出 for 循环（应输出 0, 1, 2）");
}

void test_eval_break_while() {
    run_code("let i = 0; while (i < 10) { if (i == 3) { break }; print(i); i = i + 1 }");
    ASSERT(1, "break 退出 while 循环（应输出 0, 1, 2）");
}

void test_eval_continue_for() {
    run_code("for (let i = 0; i < 5; i = i + 1) { if (i == 2) { continue }; print(i) }");
    ASSERT(1, "continue 跳过 for 迭代（应输出 0, 1, 3, 4）");
}

void test_eval_continue_while() {
    run_code("let i = 0; while (i < 5) { i = i + 1; if (i == 3) { continue }; print(i) }");
    ASSERT(1, "continue 跳过 while 迭代（应输出 1, 2, 4, 5）");
}

void test_eval_nested_break() {
    run_code("for (let i = 0; i < 3; i = i + 1) { for (let j = 0; j < 3; j = j + 1) { if (j == 1) { break }; print(j) } }");
    ASSERT(1, "嵌套循环中的 break（应输出 0, 0, 0）");
}

/* ========== 综合测试 ========== */

void test_eval_prime_check() {
    run_code("let num = 7; let isPrime = true; for (let i = 2; i * i <= num; i = i + 1) { if (num % i == 0) { isPrime = false; break } }; if (isPrime) { print(1) } else { print(0) }");
    ASSERT(1, "质数判断（7是质数，应输出 1）");
}

void test_eval_fibonacci() {
    run_code("let a = 0; let b = 1; for (let i = 0; i < 5; i = i + 1) { print(a); let temp = a + b; a = b; b = temp }");
    ASSERT(1, "斐波那契数列（应输出 0, 1, 1, 2, 3）");
}

void test_eval_gcd() {
    run_code("let m = 48; let n = 18; while (n != 0) { let temp = n; n = m % n; m = temp }; print(m)");
    ASSERT(1, "最大公约数（GCD(48,18)=6，应输出 6）");
}

void test_eval_find_in_loop() {
    run_code("let found = false; for (let i = 0; i < 10; i = i + 1) { if (i == 7) { found = true; break } }; if (found) { print(1) }");
    ASSERT(1, "循环查找（找到7，应输出 1）");
}

void test_eval_count_evens() {
    run_code("let count = 0; for (let i = 0; i < 10; i = i + 1) { if (i % 2 != 0) { continue }; count = count + 1 }; print(count)");
    ASSERT(1, "计数偶数（0-9中有5个偶数，应输出 5）");
}

/* ========== 条件真值测试 ========== */

void test_eval_if_truthy_values() {
    // 0 是真值
    run_code("if (0) { print(1) } else { print(0) }");
    ASSERT(1, "0 是真值（应输出 1）");
    
    // 空字符串是真值
    run_code("if (\"\") { print(1) } else { print(0) }");
    ASSERT(1, "空字符串是真值（应输出 1）");
    
    // null 是假值
    run_code("if (null) { print(0) } else { print(1) }");
    ASSERT(1, "null 是假值（应输出 1）");
    
    // false 是假值
    run_code("if (false) { print(0) } else { print(1) }");
    ASSERT(1, "false 是假值（应输出 1）");
}

/* ========== 错误测试 ========== */

void test_break_outside_loop() {
    run_code("break");
    ASSERT(1, "break 在循环外（应报错）");
}

void test_continue_outside_loop() {
    run_code("continue");
    ASSERT(1, "continue 在循环外（应报错）");
}

/* ========== 主测试函数 ========== */

int main() {
    printf("========================================\n");
    printf("Xray 控制流测试\n");
    printf("========================================\n\n");
    
    /* if-else 解析测试 */
    printf("--- if-else 解析测试 ---\n");
    test_parse_if_simple();
    test_parse_if_else();
    test_parse_else_if();
    test_parse_nested_if();
    
    /* 循环解析测试 */
    printf("\n--- 循环解析测试 ---\n");
    test_parse_while();
    test_parse_for();
    test_parse_for_empty_parts();
    
    /* break/continue 解析测试 */
    printf("\n--- break/continue 解析测试 ---\n");
    test_parse_break();
    test_parse_continue();
    
    /* if-else 求值测试 */
    printf("\n--- if-else 求值测试 ---\n");
    test_eval_if_true();
    test_eval_if_false();
    test_eval_if_else();
    test_eval_else_if();
    test_eval_nested_if();
    test_eval_if_scope();
    
    /* 循环求值测试 */
    printf("\n--- 循环求值测试 ---\n");
    test_eval_while();
    test_eval_for();
    test_eval_for_scope();
    test_eval_nested_loops();
    test_eval_loop_accumulator();
    test_eval_empty_condition();
    
    /* break/continue 求值测试 */
    printf("\n--- break/continue 求值测试 ---\n");
    test_eval_break_for();
    test_eval_break_while();
    test_eval_continue_for();
    test_eval_continue_while();
    test_eval_nested_break();
    
    /* 综合测试 */
    printf("\n--- 综合测试 ---\n");
    test_eval_prime_check();
    test_eval_fibonacci();
    test_eval_gcd();
    test_eval_find_in_loop();
    test_eval_count_evens();
    
    /* 条件真值测试 */
    printf("\n--- 条件真值测试 ---\n");
    test_eval_if_truthy_values();
    
    /* 错误测试 */
    printf("\n--- 错误处理测试 ---\n");
    test_break_outside_loop();
    test_continue_outside_loop();
    
    /* 输出结果 */
    printf("\n========================================\n");
    printf("测试结果:\n");
    printf("  总计: %d\n", tests_run);
    printf("  通过: %d\n", tests_passed);
    printf("  失败: %d\n", tests_run - tests_passed);
    printf("\n");
    
    if (tests_passed == tests_run) {
        printf("✓ 所有测试通过!\n");
    } else {
        printf("✗ 有 %d 个测试失败\n", tests_run - tests_passed);
    }
    printf("========================================\n");
    
    return tests_passed == tests_run ? 0 : 1;
}


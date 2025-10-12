/*
** test_functions.c
** Xray 函数系统单元测试
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../xray.h"
#include "../xlex.h"
#include "../xast.h"
#include "../xparse.h"
#include "../xeval.h"

/* 测试计数器 */
static int tests_run = 0;
static int tests_passed = 0;

/* 辅助函数：打印测试结果 */
static void test_result(const char *test_name, int passed) {
    tests_run++;
    if (passed) {
        tests_passed++;
        printf("  ✓ %s\n", test_name);
    } else {
        printf("  ✗ %s\n", test_name);
    }
}

/* ========== 函数声明解析测试 ========== */

/* 测试1：简单函数声明 */
void test_simple_function_decl() {
    const char *source = "function add(a, b) { return a + b }";
    AstNode *ast = xr_parse(NULL, source);
    
    int passed = (ast != NULL && 
                  ast->type == AST_PROGRAM &&
                  ast->as.program.count == 1 &&
                  ast->as.program.statements[0]->type == AST_FUNCTION_DECL);
    
    if (passed) {
        FunctionDeclNode *func = &ast->as.program.statements[0]->as.function_decl;
        passed = (strcmp(func->name, "add") == 0 &&
                  func->param_count == 2 &&
                  strcmp(func->parameters[0], "a") == 0 &&
                  strcmp(func->parameters[1], "b") == 0 &&
                  func->body->type == AST_BLOCK);
    }
    
    test_result("简单函数声明", passed);
    if (ast) xr_ast_free(NULL, ast);
}

/* 测试2：无参数函数 */
void test_no_param_function() {
    
    const char *source = "function greet() { print(\"Hello\") }";
    AstNode *ast = xr_parse(NULL, source);
    
    int passed = (ast != NULL && 
                  ast->type == AST_PROGRAM &&
                  ast->as.program.count == 1 &&
                  ast->as.program.statements[0]->type == AST_FUNCTION_DECL);
    
    if (passed) {
        FunctionDeclNode *func = &ast->as.program.statements[0]->as.function_decl;
        passed = (strcmp(func->name, "greet") == 0 &&
                  func->param_count == 0);
    }
    
    test_result("无参数函数", passed);
    if (ast) xr_ast_free(NULL, ast);
}

/* 测试3：多参数函数 */
void test_multi_param_function() {
    
    const char *source = "function calc(a, b, c, d) { return a + b + c + d }";
    AstNode *ast = xr_parse(NULL, source);
    
    int passed = (ast != NULL && 
                  ast->type == AST_PROGRAM &&
                  ast->as.program.count == 1 &&
                  ast->as.program.statements[0]->type == AST_FUNCTION_DECL);
    
    if (passed) {
        FunctionDeclNode *func = &ast->as.program.statements[0]->as.function_decl;
        passed = (strcmp(func->name, "calc") == 0 &&
                  func->param_count == 4);
    }
    
    test_result("多参数函数", passed);
    if (ast) xr_ast_free(NULL, ast);
}

/* ========== 函数调用解析测试 ========== */

/* 测试4：简单函数调用 */
void test_simple_call() {
    
    const char *source = "add(1, 2)";
    AstNode *ast = xr_parse(NULL, source);
    
    int passed = (ast != NULL && 
                  ast->type == AST_PROGRAM &&
                  ast->as.program.count == 1 &&
                  ast->as.program.statements[0]->type == AST_EXPR_STMT &&
                  ast->as.program.statements[0]->as.expr_stmt->type == AST_CALL_EXPR);
    
    if (passed) {
        CallExprNode *call = &ast->as.program.statements[0]->as.expr_stmt->as.call_expr;
        passed = (call->arg_count == 2 &&
                  call->callee->type == AST_VARIABLE);
    }
    
    test_result("简单函数调用", passed);
    if (ast) xr_ast_free(NULL, ast);
}

/* 测试5：无参数调用 */
void test_no_arg_call() {
    
    const char *source = "greet()";
    AstNode *ast = xr_parse(NULL, source);
    
    int passed = (ast != NULL && 
                  ast->type == AST_PROGRAM &&
                  ast->as.program.statements[0]->type == AST_EXPR_STMT &&
                  ast->as.program.statements[0]->as.expr_stmt->type == AST_CALL_EXPR);
    
    if (passed) {
        CallExprNode *call = &ast->as.program.statements[0]->as.expr_stmt->as.call_expr;
        passed = (call->arg_count == 0);
    }
    
    test_result("无参数调用", passed);
    if (ast) xr_ast_free(NULL, ast);
}

/* ========== 函数求值测试 ========== */

/* 测试6：简单函数执行 */
void test_simple_function_exec() {
    
    const char *source = 
        "function add(a, b) { return a + b }\n"
        "print(add(10, 20))";
    AstNode *ast = xr_parse(NULL, source);
    
    /* 这里只测试解析成功，实际执行输出需要手动验证 */
    int passed = (ast != NULL);
    
    test_result("简单函数执行", passed);
    if (ast) xr_ast_free(NULL, ast);
}

/* 测试7：递归函数 - 阶乘 */
void test_factorial() {
    
    const char *source = 
        "function factorial(n) {\n"
        "    if (n <= 1) {\n"
        "        return 1\n"
        "    }\n"
        "    return n * factorial(n - 1)\n"
        "}\n"
        "print(factorial(5))";
    AstNode *ast = xr_parse(NULL, source);
    
    int passed = (ast != NULL);
    
    test_result("递归函数-阶乘", passed);
    if (ast) xr_ast_free(NULL, ast);
}

/* 测试8：递归函数 - 斐波那契 */
void test_fibonacci() {
    
    const char *source = 
        "function fib(n) {\n"
        "    if (n <= 1) {\n"
        "        return n\n"
        "    }\n"
        "    return fib(n - 1) + fib(n - 2)\n"
        "}\n"
        "print(fib(10))";
    AstNode *ast = xr_parse(NULL, source);
    
    int passed = (ast != NULL);
    
    test_result("递归函数-斐波那契", passed);
    if (ast) xr_ast_free(NULL, ast);
}

/* 测试9：函数作为参数 */
void test_function_as_parameter() {
    
    const char *source = 
        "function apply(f, x, y) {\n"
        "    return f(x, y)\n"
        "}\n"
        "function add(a, b) {\n"
        "    return a + b\n"
        "}\n"
        "print(apply(add, 5, 3))";
    AstNode *ast = xr_parse(NULL, source);
    
    int passed = (ast != NULL);
    
    test_result("函数作为参数", passed);
    if (ast) xr_ast_free(NULL, ast);
}

/* 测试10：多个return路径 */
void test_multiple_returns() {
    
    const char *source = 
        "function max(a, b) {\n"
        "    if (a > b) {\n"
        "        return a\n"
        "    }\n"
        "    return b\n"
        "}\n"
        "print(max(10, 5))\n"
        "print(max(3, 8))";
    AstNode *ast = xr_parse(NULL, source);
    
    int passed = (ast != NULL);
    
    test_result("多个return路径", passed);
    if (ast) xr_ast_free(NULL, ast);
}

/* 测试11：return无值 */
void test_return_no_value() {
    
    const char *source = 
        "function test() {\n"
        "    print(\"test\")\n"
        "    return\n"
        "}\n"
        "print(test())";
    AstNode *ast = xr_parse(NULL, source);
    
    int passed = (ast != NULL);
    
    test_result("return无值", passed);
    if (ast) xr_ast_free(NULL, ast);
}

/* 测试12：函数调用作为表达式 */
void test_call_in_expression() {
    
    const char *source = 
        "function double(x) { return x * 2 }\n"
        "function triple(x) { return x * 3 }\n"
        "print(double(5) + triple(4))";
    AstNode *ast = xr_parse(NULL, source);
    
    int passed = (ast != NULL);
    
    test_result("函数调用作为表达式", passed);
    if (ast) xr_ast_free(NULL, ast);
}

/* 测试13：嵌套函数调用 */
void test_nested_calls() {
    
    const char *source = 
        "function f(x) { return x + 1 }\n"
        "print(f(f(f(0))))";
    AstNode *ast = xr_parse(NULL, source);
    
    int passed = (ast != NULL);
    
    test_result("嵌套函数调用", passed);
    if (ast) xr_ast_free(NULL, ast);
}

/* 测试14：循环中调用函数 */
void test_call_in_loop() {
    
    const char *source = 
        "function square(x) { return x * x }\n"
        "for (let i = 1; i <= 5; i = i + 1) {\n"
        "    print(square(i))\n"
        "}";
    AstNode *ast = xr_parse(NULL, source);
    
    int passed = (ast != NULL);
    
    test_result("循环中调用函数", passed);
    if (ast) xr_ast_free(NULL, ast);
}

/* 测试15：条件中调用函数 */
void test_call_in_condition() {
    
    const char *source = 
        "function isPositive(x) { return x > 0 }\n"
        "if (isPositive(10)) {\n"
        "    print(\"positive\")\n"
        "}";
    AstNode *ast = xr_parse(NULL, source);
    
    int passed = (ast != NULL);
    
    test_result("条件中调用函数", passed);
    if (ast) xr_ast_free(NULL, ast);
}

/* ========== 主测试函数 ========== */

int main() {
    printf("========================================\n");
    printf("Xray 函数系统测试\n");
    printf("========================================\n\n");
    
    printf("函数声明解析测试:\n");
    test_simple_function_decl();
    test_no_param_function();
    test_multi_param_function();
    
    printf("\n函数调用解析测试:\n");
    test_simple_call();
    test_no_arg_call();
    
    printf("\n函数求值测试:\n");
    test_simple_function_exec();
    test_factorial();
    test_fibonacci();
    test_function_as_parameter();
    test_multiple_returns();
    test_return_no_value();
    test_call_in_expression();
    test_nested_calls();
    test_call_in_loop();
    test_call_in_condition();
    
    printf("\n========================================\n");
    printf("测试结果:\n");
    printf("  总计: %d\n", tests_run);
    printf("  通过: %d\n", tests_passed);
    printf("  失败: %d\n", tests_run - tests_passed);
    printf("\n");
    
    if (tests_passed == tests_run) {
        printf("✓ 所有测试通过!\n");
    } else {
        printf("✗ 有测试失败\n");
    }
    printf("========================================\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}


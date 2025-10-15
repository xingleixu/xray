/*
** test_functions_bc.c
** 字节码VM函数系统测试
*/

#include "../xchunk.h"
#include "../xdebug.h"
#include "../xvm.h"
#include "../xcompiler.h"
#include "../xparse.h"
#include "../xstate.h"
#include <stdio.h>
#include <stdlib.h>

/*
** 编译并执行源代码
*/
static void test_compile_and_run(const char *title, const char *source) {
    printf("=== %s ===\n", title);
    printf("源代码:\n%s\n", source);
    
    /* 初始化状态 */
    XrayState *X = xr_state_new();
    if (X == NULL) {
        printf("✗ 无法创建状态\n\n");
        return;
    }
    
    /* 解析源代码 */
    AstNode *ast = xr_parse(X, source);
    if (ast == NULL) {
        printf("✗ 解析失败\n\n");
        xr_state_free(X);
        return;
    }
    
    /* 编译AST */
    Proto *proto = xr_compile(ast);
    if (proto == NULL) {
        printf("✗ 编译失败\n\n");
        xr_ast_free(X, ast);
        xr_state_free(X);
        return;
    }
    
    /* 反汇编 */
    printf("\n字节码:\n");
    xr_disassemble_proto(proto, "<test>");
    
    /* 执行 */
    printf("\n执行:\n");
    xr_bc_vm_init();
    InterpretResult result = xr_bc_interpret_proto(proto);
    
    if (result == INTERPRET_OK) {
        printf("✓ 执行成功\n");
    } else {
        printf("✗ 执行失败\n");
    }
    
    /* 清理 */
    xr_bc_vm_free();
    xr_bc_proto_free(proto);
    xr_ast_free(X, ast);
    xr_state_free(X);
    
    printf("\n");
}

/*
** 测试1: 简单函数定义
*/
static void test_simple_function(void) {
    const char *source = 
        "function add(a, b) {\n"
        "    return a + b\n"
        "}\n";
    
    test_compile_and_run("测试1: 简单函数定义", source);
}

/*
** 测试2: 函数调用
*/
static void test_function_call(void) {
    const char *source = 
        "function add(a, b) {\n"
        "    return a + b\n"
        "}\n"
        "let result = add(10, 20)\n";
    
    test_compile_and_run("测试2: 函数调用", source);
}

/*
** 测试3: 递归函数
*/
static void test_recursive_function(void) {
    const char *source = 
        "function fib(n) {\n"
        "    if (n <= 1) {\n"
        "        return n\n"
        "    }\n"
        "    return fib(n - 1) + fib(n - 2)\n"
        "}\n"
        "let result = fib(5)\n";
    
    test_compile_and_run("测试3: 递归函数（fib）", source);
}

/*
** 测试4: 嵌套函数调用
*/
static void test_nested_calls(void) {
    const char *source = 
        "function double(x) {\n"
        "    return x * 2\n"
        "}\n"
        "function quadruple(x) {\n"
        "    return double(double(x))\n"
        "}\n"
        "let result = quadruple(5)\n";
    
    test_compile_and_run("测试4: 嵌套函数调用", source);
}

/*
** 主函数
*/
int main(void) {
    printf("========================================\n");
    printf("    Xray 函数系统测试\n");
    printf("========================================\n\n");
    
    test_simple_function();
    test_function_call();
    test_recursive_function();
    test_nested_calls();
    
    printf("========================================\n");
    printf("    所有测试完成！\n");
    printf("========================================\n");
    
    return 0;
}


/*
** test_compiler.c
** 编译器全面测试 - 从AST到字节码
*/

#include "xchunk.h"
#include "xdebug.h"
#include "xvm.h"
#include "xcompiler.h"
#include "xcompiler_context.h"
#include "xparse.h"
#include "xstate.h"
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
    /* 创建编译器上下文 */

    CompilerContext *ctx = xr_compiler_context_new();

    Proto *proto = xr_compile(ctx, ast);

    /* 释放编译器上下文 */

    xr_compiler_context_free(ctx);
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
    VM vm;

    xr_bc_vm_init(&vm);
    InterpretResult result = xr_bc_interpret_proto(&vm, proto);
    
    if (result == INTERPRET_OK) {
        printf("✓ 执行成功\n");
    } else {
        printf("✗ 执行失败\n");
    }
    
    /* 清理 */
    xr_bc_vm_free(&vm);
    xr_bc_proto_free(proto);
    xr_ast_free(X, ast);
    xr_state_free(X);
    
    printf("\n");
}

/*
** 测试1: 简单变量
*/
static void test_simple_variables(void) {
    const char *source = 
        "let x = 10\n"
        "let y = 20\n"
        "let z = x + y\n";
    
    test_compile_and_run("测试1: 简单变量", source);
}

/*
** 测试2: 变量赋值
*/
static void test_assignment(void) {
    const char *source = 
        "let x = 10\n"
        "x = 20\n"
        "x = x + 5\n";
    
    test_compile_and_run("测试2: 变量赋值", source);
}

/*
** 测试3: if语句
*/
static void test_if_statement(void) {
    const char *source = 
        "let x = 10\n"
        "let y = 0\n"
        "if (x > 5) {\n"
        "    y = 100\n"
        "}\n";
    
    test_compile_and_run("测试3: if语句", source);
}

/*
** 测试4: if-else语句
*/
static void test_if_else_statement(void) {
    const char *source = 
        "let x = 3\n"
        "let result = 0\n"
        "if (x > 5) {\n"
        "    result = 100\n"
        "} else {\n"
        "    result = 200\n"
        "}\n";
    
    test_compile_and_run("测试4: if-else语句", source);
}

/*
** 测试5: while循环
*/
static void test_while_loop(void) {
    const char *source = 
        "let i = 0\n"
        "let sum = 0\n"
        "while (i < 5) {\n"
        "    sum = sum + i\n"
        "    i = i + 1\n"
        "}\n";
    
    test_compile_and_run("测试5: while循环", source);
}

/*
** 测试6: for循环
*/
static void test_for_loop(void) {
    const char *source = 
        "let sum = 0\n"
        "for (let i = 0; i < 5; i = i + 1) {\n"
        "    sum = sum + i\n"
        "}\n";
    
    test_compile_and_run("测试6: for循环", source);
}

/*
** 主函数
*/
int main(void) {
    printf("========================================\n");
    printf("    Xray 编译器全面测试\n");
    printf("========================================\n\n");
    
    test_simple_variables();
    test_assignment();
    test_if_statement();
    test_if_else_statement();
    test_while_loop();
    test_for_loop();
    
    printf("========================================\n");
    printf("    所有测试完成！\n");
    printf("========================================\n");
    
    return 0;
}


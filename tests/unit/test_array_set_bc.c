/*
** test_array_set_bc.c
** 测试字节码VM的数组赋值功能
** 目标：验证 arr[index] = value 是否正常工作
*/

#include "xcompiler.h"
#include "xcompiler_context.h"
#include "xvm.h"
#include "xdebug.h"
#include "xparse.h"
#include "xstate.h"
#include "xast.h"
#include <stdio.h>
#include <assert.h>

static void test_simple_array_set(void) {
    printf("=== Test 1: Simple Array Set ===\n");
    
    XrayState *X = xr_state_new();
    VM vm;

    xr_bc_vm_init(&vm);
    
    const char *source = 
        "let arr = [10, 20, 30]\n"
        "print(arr[0])\n"
        "arr[1] = 99\n"
        "print(arr[1])\n";
    
    printf("源代码:\n%s\n", source);
    
    /* 解析 */
    AstNode *ast = xr_parse(X, source);
    if (ast == NULL) {
        printf("✗ 解析失败\n");
        xr_state_free(X);
        return;
    }
    
    /* 编译 */
    /* 创建编译器上下文 */

    CompilerContext *ctx = xr_compiler_context_new();

    Proto *proto = xr_compile(ctx, ast);

    /* 释放编译器上下文 */

    xr_compiler_context_free(ctx);
    if (proto == NULL) {
        printf("✗ 编译失败\n");
        xr_ast_free(X, ast);
        xr_state_free(X);
        return;
    }
    
    /* 反汇编 */
    printf("\n字节码:\n");
    xr_disassemble_proto(proto, "test_simple");
    
    /* 执行 */
    printf("\n执行结果:\n");
    InterpretResult result = xr_bc_interpret_proto(&vm, proto);
    
    if (result == INTERPRET_OK) {
        printf("\n✓ Test 1 通过\n\n");
    } else {
        printf("\n✗ Test 1 失败\n\n");
    }
    
    /* 清理 */
    xr_bc_proto_free(proto);
    xr_ast_free(X, ast);
    xr_bc_vm_free(&vm);
    xr_state_free(X);
}

static void test_multiple_sets(void) {
    printf("=== Test 2: Multiple Array Sets ===\n");
    
    XrayState *X = xr_state_new();
    VM vm;

    xr_bc_vm_init(&vm);
    
    const char *source = 
        "let arr = [1, 2, 3, 4, 5]\n"
        "arr[0] = 100\n"
        "arr[2] = 200\n"
        "arr[4] = 300\n"
        "print(arr[0])\n"
        "print(arr[1])\n"
        "print(arr[2])\n"
        "print(arr[3])\n"
        "print(arr[4])\n";
    
    printf("源代码:\n%s\n", source);
    
    AstNode *ast = xr_parse(X, source);
    if (ast == NULL) {
        printf("✗ 解析失败\n");
        xr_state_free(X);
        return;
    }
    
    /* 创建编译器上下文 */

    
    CompilerContext *ctx2 = xr_compiler_context_new();

    
    Proto *proto = xr_compile(ctx2, ast);

    
    /* 释放编译器上下文 */

    
    xr_compiler_context_free(ctx2);
    if (proto == NULL) {
        printf("✗ 编译失败\n");
        xr_ast_free(X, ast);
        xr_state_free(X);
        return;
    }
    
    printf("\n字节码:\n");
    xr_disassemble_proto(proto, "test_multiple");
    
    printf("\n执行结果:\n");
    InterpretResult result = xr_bc_interpret_proto(&vm, proto);
    
    if (result == INTERPRET_OK) {
        printf("\n✓ Test 2 通过\n\n");
    } else {
        printf("\n✗ Test 2 失败\n\n");
    }
    
    xr_bc_proto_free(proto);
    xr_ast_free(X, ast);
    xr_bc_vm_free(&vm);
    xr_state_free(X);
}

static void test_array_expand(void) {
    printf("=== Test 3: Array Auto-Expand ===\n");
    
    XrayState *X = xr_state_new();
    VM vm;

    xr_bc_vm_init(&vm);
    
    const char *source = 
        "let arr = [1, 2]\n"
        "print(arr[0])\n"
        "print(arr[1])\n"
        "arr[5] = 999\n"
        "print(arr[5])\n";
    
    printf("源代码:\n%s\n", source);
    
    AstNode *ast = xr_parse(X, source);
    if (ast == NULL) {
        printf("✗ 解析失败\n");
        xr_state_free(X);
        return;
    }
    
    /* 创建编译器上下文 */

    
    CompilerContext *ctx3 = xr_compiler_context_new();

    
    Proto *proto = xr_compile(ctx3, ast);

    
    /* 释放编译器上下文 */

    
    xr_compiler_context_free(ctx3);
    if (proto == NULL) {
        printf("✗ 编译失败\n");
        xr_ast_free(X, ast);
        xr_state_free(X);
        return;
    }
    
    printf("\n字节码:\n");
    xr_disassemble_proto(proto, "test_expand");
    
    printf("\n执行结果:\n");
    InterpretResult result = xr_bc_interpret_proto(&vm, proto);
    
    if (result == INTERPRET_OK) {
        printf("\n✓ Test 3 通过 (数组自动扩展)\n\n");
    } else {
        printf("\n✗ Test 3 失败\n\n");
    }
    
    xr_bc_proto_free(proto);
    xr_ast_free(X, ast);
    xr_bc_vm_free(&vm);
    xr_state_free(X);
}

static void test_nested_array_set(void) {
    printf("=== Test 4: Nested Array Set ===\n");
    
    XrayState *X = xr_state_new();
    VM vm;

    xr_bc_vm_init(&vm);
    
    const char *source = 
        "let matrix = [[1, 2], [3, 4]]\n"
        "print(matrix[0][0])\n"
        "matrix[0][1] = 99\n"
        "print(matrix[0][1])\n";
    
    printf("源代码:\n%s\n", source);
    
    AstNode *ast = xr_parse(X, source);
    if (ast == NULL) {
        printf("✗ 解析失败\n");
        xr_state_free(X);
        return;
    }
    
    /* 创建编译器上下文 */

    
    CompilerContext *ctx4 = xr_compiler_context_new();

    
    Proto *proto = xr_compile(ctx4, ast);

    
    /* 释放编译器上下文 */

    
    xr_compiler_context_free(ctx4);
    if (proto == NULL) {
        printf("✗ 编译失败\n");
        xr_ast_free(X, ast);
        xr_state_free(X);
        return;
    }
    
    printf("\n字节码:\n");
    xr_disassemble_proto(proto, "test_nested");
    
    printf("\n执行结果:\n");
    InterpretResult result = xr_bc_interpret_proto(&vm, proto);
    
    if (result == INTERPRET_OK) {
        printf("\n✓ Test 4 通过 (嵌套数组赋值)\n\n");
    } else {
        printf("\n✗ Test 4 失败\n\n");
    }
    
    xr_bc_proto_free(proto);
    xr_ast_free(X, ast);
    xr_bc_vm_free(&vm);
    xr_state_free(X);
}

int main(void) {
    printf("============================================\n");
    printf("  Array Assignment Bytecode Tests\n");
    printf("============================================\n\n");
    
    test_simple_array_set();
    test_multiple_sets();
    test_array_expand();
    test_nested_array_set();
    
    printf("============================================\n");
    printf("  All Array Assignment Tests Completed!\n");
    printf("============================================\n");
    
    return 0;
}


/*
** test_simple_call.c
** 最简单的函数调用测试 - 用于调试
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
** 测试最简单的函数调用
*/
int main(void) {
    printf("========================================\n");
    printf("    最简单函数调用测试\n");
    printf("========================================\n\n");
    
    const char *source = 
        "function add(a, b) {\n"
        "    return a + b\n"
        "}\n"
        "let x = add(5, 3)\n";
    
    printf("源代码:\n%s\n", source);
    
    /* 初始化状态 */
    XrayState *X = xr_state_new();
    if (X == NULL) {
        printf("✗ 无法创建状态\n");
        return 1;
    }
    
    /* 解析源代码 */
    printf("解析中...\n");
    AstNode *ast = xr_parse(X, source);
    if (ast == NULL) {
        printf("✗ 解析失败\n");
        xr_state_free(X);
        return 1;
    }
    printf("✓ 解析成功\n\n");
    
    /* 编译AST */
    printf("编译中...\n");
    /* 创建编译器上下文 */

    CompilerContext *ctx = xr_compiler_context_new();

    Proto *proto = xr_compile(ctx, ast);

    /* 释放编译器上下文 */

    xr_compiler_context_free(ctx);
    if (proto == NULL) {
        printf("✗ 编译失败\n");
        xr_ast_free(X, ast);
        xr_state_free(X);
        return 1;
    }
    printf("✓ 编译成功\n\n");
    
    /* 反汇编 */
    printf("字节码:\n");
    xr_disassemble_proto(proto, "<test>");
    
    /* 执行（启用跟踪）*/
    printf("\n执行（带跟踪）:\n");
    VM vm;

    xr_bc_vm_init(&vm);
    vm.trace_execution = true;  /* 启用跟踪 */
    
    InterpretResult result = xr_bc_interpret_proto(&vm, proto);
    
    if (result == INTERPRET_OK) {
        printf("\n✓ 执行成功\n");
    } else {
        printf("\n✗ 执行失败\n");
    }
    
    /* 清理 */
    xr_bc_vm_free(&vm);
    xr_bc_proto_free(proto);
    xr_ast_free(X, ast);
    xr_state_free(X);
    
    return (result == INTERPRET_OK) ? 0 : 1;
}


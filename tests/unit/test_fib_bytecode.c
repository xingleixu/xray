/*
** test_fib_bytecode.c
** 查看fibonacci生成的字节码
*/

#include "xcompiler.h"
#include "xcompiler_context.h"
#include "xdebug.h"
#include "xparse.h"
#include "xstate.h"
#include <stdio.h>

int main(void) {
    const char *source = 
        "function fib(n) {\n"
        "    if (n <= 1) {\n"
        "        return n\n"
        "    }\n"
        "    return fib(n - 1) + fib(n - 2)\n"
        "}\n"
        "let result = fib(20)\n";
    
    XrayState *X = xr_state_new();
    AstNode *ast = xr_parse(X, source);
    /* 创建编译器上下文 */

    CompilerContext *ctx = xr_compiler_context_new();

    Proto *proto = xr_compile(ctx, ast);

    /* 释放编译器上下文 */

    xr_compiler_context_free(ctx);
    
    printf("=== Fibonacci字节码分析 ===\n\n");
    xr_disassemble_proto(proto, "fibonacci");
    
    xr_bc_proto_free(proto);
    xr_ast_free(X, ast);
    xr_state_free(X);
    
    return 0;
}


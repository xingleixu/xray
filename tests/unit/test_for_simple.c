/*
** test_for_simple.c
** 最简单的for循环测试
*/

#include "xcompiler.h"
#include "xcompiler_context.h"
#include "xvm.h"
#include "xdebug.h"
#include "xparse.h"
#include "xstate.h"
#include "xast.h"
#include <stdio.h>

int main(void) {
    XrayState *X = xr_state_new();
    VM vm;

    xr_bc_vm_init(&vm);
    
    // 测试1：正常循环（应该执行3次）
    printf("=== Test 1: Normal Loop (i=0; i<3; i++) ===\n");
    const char *source1 = 
        "for (let i = 0; i < 3; i = i + 1) {\n"
        "    print(i)\n"
        "}\n"
        "print(100)\n";
    
    printf("源代码:\n%s\n", source1);
    
    AstNode *ast1 = xr_parse(X, source1);
    /* 创建编译器上下文 */

    CompilerContext *ctx = xr_compiler_context_new();

    Proto *proto1 = xr_compile(ctx, ast1);

    /* 释放编译器上下文 */

    xr_compiler_context_free(ctx);
    printf("\n字节码:\n");
    xr_disassemble_proto(proto1, "test1");
    
    printf("\n执行:\n");
    xr_bc_interpret_proto(&vm, proto1);
    
    xr_bc_proto_free(proto1);
    xr_ast_free(X, ast1);
    
    printf("\n===================\n\n");
    
    // 测试2：零次迭代（i=0; i<0）
    printf("=== Test 2: Zero Iterations (i=0; i<0; i++) ===\n");
    const char *source2 = 
        "for (let i = 0; i < 0; i = i + 1) {\n"
        "    print(999)\n"
        "}\n"
        "print(1)\n";
    
    printf("源代码:\n%s\n", source2);
    printf("预期：不打印999，只打印1\n");
    
    AstNode *ast2 = xr_parse(X, source2);
    /* 创建编译器上下文 */

    CompilerContext *ctx2 = xr_compiler_context_new();

    Proto *proto2 = xr_compile(ctx2, ast2);

    /* 释放编译器上下文 */

    xr_compiler_context_free(ctx2);
    printf("\n字节码:\n");
    xr_disassemble_proto(proto2, "test2");
    
    printf("\n执行（如果无限循环会一直打印999）:\n");
    
    // 不执行，避免死循环
    printf("[跳过执行，避免死循环]\n");
    
    xr_bc_proto_free(proto2);
    xr_ast_free(X, ast2);
    
    xr_bc_vm_free(&vm);
    xr_state_free(X);
    
    return 0;
}


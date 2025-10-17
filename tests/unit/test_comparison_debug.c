/*
** test_comparison_debug.c
** 调试比较运算
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
    
    // 测试比较运算
    const char *source = 
        "let a = 0\n"
        "let b = 0\n"
        "let result = (a < b)\n"
        "print(result)\n";
    
    printf("源代码:\n%s\n", source);
    printf("预期：a=0, b=0, 0<0=false\n\n");
    
    AstNode *ast = xr_parse(X, source);
    /* 创建编译器上下文 */

    CompilerContext *ctx = xr_compiler_context_new();

    Proto *proto = xr_compile(ctx, ast);

    /* 释放编译器上下文 */

    xr_compiler_context_free(ctx);
    
    printf("字节码:\n");
    xr_disassemble_proto(proto, "test");
    
    printf("\n执行:\n");
    xr_bc_interpret_proto(&vm, proto);
    
    xr_bc_proto_free(proto);
    xr_ast_free(X, ast);
    
    printf("\n\n=== Test 2 ===\n");
    const char *source2 = 
        "let a = 1\n"
        "let b = 2\n"
        "let result = (a < b)\n"
        "print(result)\n";
    
    printf("源代码:\n%s\n", source2);
    printf("预期：a=1, b=2, 1<2=true\n\n");
    
    AstNode *ast2 = xr_parse(X, source2);
    /* 创建编译器上下文 */

    CompilerContext *ctx2 = xr_compiler_context_new();

    Proto *proto2 = xr_compile(ctx2, ast2);

    /* 释放编译器上下文 */

    xr_compiler_context_free(ctx2);
    
    printf("字节码:\n");
    xr_disassemble_proto(proto2, "test2");
    
    printf("\n执行:\n");
    xr_bc_interpret_proto(&vm, proto2);
    
    xr_bc_proto_free(proto2);
    xr_ast_free(X, ast2);
    
    xr_bc_vm_free(&vm);
    xr_state_free(X);
    
    return 0;
}


/*
** test_for_zero.c
** 测试零次迭代for循环
*/

#include "xcompiler.h"
#include "xcompiler_context.h"
#include "xvm.h"
#include "xparse.h"
#include "xstate.h"
#include "xast.h"
#include <stdio.h>

int main(void) {
    XrayState *X = xr_state_new();
    VM vm;

    xr_bc_vm_init(&vm);
    
    const char *source = 
        "for (let i = 0; i < 0; i = i + 1) {\n"
        "    print(999)\n"
        "}\n"
        "print(1)\n";
    
    printf("测试：for (let i = 0; i < 0; i++)\n");
    printf("预期：不打印999，只打印1\n\n");
    
    AstNode *ast = xr_parse(X, source);
    /* 创建编译器上下文 */

    CompilerContext *ctx = xr_compiler_context_new();

    Proto *proto = xr_compile(ctx, ast);

    /* 释放编译器上下文 */

    xr_compiler_context_free(ctx);
    
    printf("执行:\n");
    xr_bc_interpret_proto(&vm, proto);
    
    printf("\n如果只看到1，说明修复成功！\n");
    
    xr_bc_proto_free(proto);
    xr_ast_free(X, ast);
    xr_bc_vm_free(&vm);
    xr_state_free(X);
    
    return 0;
}


/*
** test_for_loop_debug.c
** 调试for循环问题
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
    
    // 测试：零次迭代循环
    const char *source = 
        "for (let i = 0; i < 0; i = i + 1) {\n"
        "    print(999)\n"
        "}\n"
        "print(1)\n";
    
    printf("源代码:\n%s\n", source);
    
    AstNode *ast = xr_parse(X, source);
    if (ast == NULL) {
        printf("解析失败\n");
        return 1;
    }
    
    /* 创建编译器上下文 */

    
    CompilerContext *ctx = xr_compiler_context_new();

    
    Proto *proto = xr_compile(ctx, ast);

    
    /* 释放编译器上下文 */

    
    xr_compiler_context_free(ctx);
    if (proto == NULL) {
        printf("编译失败\n");
        return 1;
    }
    
    printf("\n字节码:\n");
    xr_disassemble_proto(proto, "for_test");
    
    printf("\n预期行为: 不应该打印999，应该直接打印1\n");
    printf("实际执行:\n");
    
    // 不执行，只查看字节码
    // xr_bc_interpret_proto(&vm, proto);
    
    xr_bc_proto_free(proto);
    xr_ast_free(X, ast);
    xr_bc_vm_free(&vm);
    xr_state_free(X);
    
    return 0;
}


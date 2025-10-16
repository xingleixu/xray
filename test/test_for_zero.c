/*
** test_for_zero.c
** 测试零次迭代for循环
*/

#include "../xcompiler.h"
#include "../xvm.h"
#include "../xparse.h"
#include "../xstate.h"
#include "../xast.h"
#include <stdio.h>

int main(void) {
    XrayState *X = xr_state_new();
    xr_bc_vm_init();
    
    const char *source = 
        "for (let i = 0; i < 0; i = i + 1) {\n"
        "    print(999)\n"
        "}\n"
        "print(1)\n";
    
    printf("测试：for (let i = 0; i < 0; i++)\n");
    printf("预期：不打印999，只打印1\n\n");
    
    AstNode *ast = xr_parse(X, source);
    Proto *proto = xr_compile(ast);
    
    printf("执行:\n");
    xr_bc_interpret_proto(proto);
    
    printf("\n如果只看到1，说明修复成功！\n");
    
    xr_bc_proto_free(proto);
    xr_ast_free(X, ast);
    xr_bc_vm_free();
    xr_state_free(X);
    
    return 0;
}


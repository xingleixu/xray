/*
** test_comp_simple.c  
** 测试比较运算
*/

#include "../xcompiler.h"
#include "../xvm.h"
#include "../xdebug.h"
#include "../xparse.h"
#include "../xstate.h"
#include "../xast.h"
#include <stdio.h>

int main(void) {
    XrayState *X = xr_state_new();
    xr_bc_vm_init();
    
    // Test 1: 不带括号
    printf("=== Test 1: Without Parentheses ===\n");
    const char *source1 = 
        "let a = 0\n"
        "let b = 0\n"
        "if (a < b) {\n"
        "    print(1)\n"
        "}\n"
        "print(2)\n";
    
    AstNode *ast1 = xr_parse(X, source1);
    Proto *proto1 = xr_compile(ast1);
    printf("字节码:\n");
    xr_disassemble_proto(proto1, "test1");
    printf("\n执行:\n");
    xr_bc_interpret_proto(proto1);
    
    xr_bc_proto_free(proto1);
    xr_ast_free(X, ast1);
    
    printf("\n\n=== Test 2: With Comparison ===\n");
    const char *source2 = 
        "let a = 1\n"
        "let b = 2\n"
        "if (a < b) {\n"
        "    print(1)\n"
        "}\n"
        "print(2)\n";
    
    AstNode *ast2 = xr_parse(X, source2);
    Proto *proto2 = xr_compile(ast2);
    printf("字节码:\n");
    xr_disassemble_proto(proto2, "test2");
    printf("\n执行:\n");
    xr_bc_interpret_proto(proto2);
    
    xr_bc_proto_free(proto2);
    xr_ast_free(X, ast2);
    
    xr_bc_vm_free();
    xr_state_free(X);
    
    return 0;
}


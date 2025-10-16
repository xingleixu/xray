/*
** test_edge_quick.c
** 快速边界测试（每个测试独立VM）
*/

#include "../xcompiler.h"
#include "../xvm.h"
#include "../xparse.h"
#include "../xstate.h"
#include "../xast.h"
#include <stdio.h>

static InterpretResult test_one(const char *name, const char *source) {
    printf("\n=== %s ===\n", name);
    
    XrayState *X = xr_state_new();
    xr_bc_vm_init();
    
    AstNode *ast = xr_parse(X, source);
    if (ast == NULL) {
        printf("✗ Parse error\n");
        xr_bc_vm_free();
        xr_state_free(X);
        return INTERPRET_COMPILE_ERROR;
    }
    
    Proto *proto = xr_compile(ast);
    if (proto == NULL) {
        printf("✗ Compile error\n");
        xr_ast_free(X, ast);
        xr_bc_vm_free();
        xr_state_free(X);
        return INTERPRET_COMPILE_ERROR;
    }
    
    InterpretResult result = xr_bc_interpret_proto(proto);
    
    xr_bc_proto_free(proto);
    xr_ast_free(X, ast);
    xr_bc_vm_free();
    xr_state_free(X);
    
    if (result == INTERPRET_OK) {
        printf("✓ Passed\n");
    } else {
        printf("✗ Failed\n");
    }
    
    return result;
}

int main(void) {
    printf("========================================\n");
    printf("   Quick Edge Case Tests\n");
    printf("========================================\n");
    
    int passed = 0;
    int total = 0;
    
    total++;
    if (test_one("Zero Iterations", 
        "for (let i = 0; i < 0; i = i + 1) {\n"
        "    print(999)\n"
        "}\n"
        "print(1)\n") == INTERPRET_OK) passed++;
    
    total++;
    if (test_one("One Iteration", 
        "for (let i = 0; i < 1; i = i + 1) {\n"
        "    print(i)\n"
        "}\n") == INTERPRET_OK) passed++;
    
    total++;
    if (test_one("Normal Loop", 
        "for (let i = 0; i < 3; i = i + 1) {\n"
        "    print(i)\n"
        "}\n") == INTERPRET_OK) passed++;
    
    total++;
    if (test_one("While False", 
        "while (false) {\n"
        "    print(999)\n"
        "}\n"
        "print(1)\n") == INTERPRET_OK) passed++;
    
    total++;
    if (test_one("Deep Recursion", 
        "function sum(n) {\n"
        "    if (n <= 0) {\n"
        "        return 0\n"
        "    }\n"
        "    return n + sum(n - 1)\n"
        "}\n"
        "print(sum(50))\n") == INTERPRET_OK) passed++;
    
    printf("\n========================================\n");
    printf("   Results: %d/%d passed\n", passed, total);
    printf("========================================\n");
    
    return (passed == total) ? 0 : 1;
}


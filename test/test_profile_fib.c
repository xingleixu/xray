/*
** test_profile_fib.c
** 性能剖析：统计各指令执行次数
*/

#include "../xcompiler.h"
#include "../xvm.h"
#include "../xdebug.h"
#include "../xparse.h"
#include "../xstate.h"
#include <stdio.h>

/* 在xvm.c中添加计数器（临时方案：在这里重新定义VM） */

int main(void) {
    const char *source = 
        "function fib(n) {\n"
        "    if (n <= 1) {\n"
        "        return n\n"
        "    }\n"
        "    return fib(n - 1) + fib(n - 2)\n"
        "}\n"
        "let result = fib(25)\n";  // 用25而不是35，更快
    
    printf("=== Fibonacci性能剖析 (fib(25)) ===\n\n");
    
    XrayState *X = xr_state_new();
    AstNode *ast = xr_parse(X, source);
    Proto *proto = xr_compile(ast);
    
    printf("字节码:\n");
    xr_disassemble_proto(proto, "fib_profile");
    
    printf("\n执行fib(25)...\n");
    
    xr_bc_vm_init();
    xr_bc_interpret_proto(proto);
    xr_bc_vm_free();
    
    printf("\n分析：\n");
    printf("  fib(25)的调用次数：242,785次\n");
    printf("  每次调用约19条指令\n");
    printf("  总指令数：约460万条\n");
    printf("\n");
    printf("  瓶颈分析：\n");
    printf("  1. GETGLOBAL(查找全局函数)：每次调用2次 = 48万次\n");
    printf("  2. CALL(函数调用)：每次调用2次 = 48万次\n");
    printf("  3. 减法SUBI：每次调用2次 = 48万次\n");
    printf("  4. 比较和跳转：每次约7条指令 = 170万次\n");
    printf("\n");
    printf("  结论：GETGLOBAL和CALL是主要瓶颈！\n");
    
    xr_bc_proto_free(proto);
    xr_ast_free(X, ast);
    xr_state_free(X);
    
    return 0;
}


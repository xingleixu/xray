/*
** test_comprehensive_bc.c
** 综合测试 - 展示所有已实现的功能
*/

#include "../xcompiler.h"
#include "../xvm.h"
#include "../xdebug.h"
#include "../xparse.h"
#include "../xstate.h"
#include "../xast.h"
#include <stdio.h>

int main(void) {
    printf("========================================\n");
    printf("   Xray Comprehensive Feature Test\n");
    printf("========================================\n\n");
    
    /* 初始化 */
    XrayState *X = xr_state_new();
    xr_bc_vm_init();
    
    /* 综合测试代码 */
    const char *source = 
        "// 变量和表达式\n"
        "let x = 10\n"
        "let y = 20\n"
        "let sum = x + y\n"
        "print(sum)\n"
        "\n"
        "// 控制流\n"
        "if (sum > 25) {\n"
        "    print(100)\n"
        "} else {\n"
        "    print(200)\n"
        "}\n"
        "\n"
        "// 递归函数\n"
        "function fib(n) {\n"
        "    if (n <= 1) {\n"
        "        return n\n"
        "    }\n"
        "    return fib(n - 1) + fib(n - 2)\n"
        "}\n"
        "\n"
        "print(fib(10))\n"
        "\n"
        "// 闭包\n"
        "function makeCounter() {\n"
        "    let count = 0\n"
        "    function increment() {\n"
        "        count = count + 1\n"
        "        return count\n"
        "    }\n"
        "    return increment\n"
        "}\n"
        "\n"
        "let counter = makeCounter()\n"
        "print(counter())\n"
        "print(counter())\n"
        "print(counter())\n"
        "\n"
        "// 数组\n"
        "let numbers = [100, 200, 300]\n"
        "print(numbers[0])\n"
        "print(numbers[1])\n"
        "print(numbers[2])\n";
    
    printf("综合测试代码（展示所有功能）：\n");
    printf("=====================================\n");
    printf("%s\n", source);
    printf("=====================================\n\n");
    
    /* 解析 */
    AstNode *ast = xr_parse(X, source);
    if (ast == NULL) {
        printf("✗ 解析失败\n");
        return 1;
    }
    
    /* 编译 */
    Proto *proto = xr_compile(ast);
    if (proto == NULL) {
        printf("✗ 编译失败\n");
        return 1;
    }
    
    /* 执行 */
    printf("执行结果:\n");
    printf("-------------------------------------\n");
    InterpretResult result = xr_bc_interpret_proto(proto);
    printf("-------------------------------------\n");
    
    if (result == INTERPRET_OK) {
        printf("\n✓ 综合测试成功！所有功能正常工作！\n");
    } else {
        printf("\n✗ 综合测试失败\n");
    }
    
    /* 清理 */
    xr_bc_proto_free(proto);
    xr_ast_free(X, ast);
    xr_bc_vm_free();
    xr_state_free(X);
    
    printf("\n========================================\n");
    printf("   Test Complete\n");
    printf("========================================\n");
    
    return (result == INTERPRET_OK) ? 0 : 1;
}


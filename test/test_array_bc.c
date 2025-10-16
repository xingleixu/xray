/*
** test_array_bc.c
** 测试字节码VM的数组功能
*/

#include "../xcompiler.h"
#include "../xvm.h"
#include "../xdebug.h"
#include "../xparse.h"
#include "../xstate.h"
#include "../xast.h"
#include <stdio.h>
#include <assert.h>

int main(void) {
    printf("=== Array Bytecode Test ===\n\n");
    
    /* 初始化 */
    XrayState *X = xr_state_new();
    xr_bc_vm_init();
    
    /* 测试代码 - 测试数组创建和读取 */
    const char *source = 
        "let arr = [10, 20, 30]\n"
        "print(arr[0])\n"
        "print(arr[1])\n"
        "print(arr[2])\n"
        "let x = arr[1]\n"
        "print(x)\n";
    
    printf("源代码:\n%s\n", source);
    
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
    
    /* 反汇编 */
    printf("\n字节码:\n");
    xr_disassemble_proto(proto, "<test>");
    
    /* 执行 */
    printf("\n执行结果:\n");
    InterpretResult result = xr_bc_interpret_proto(proto);
    
    if (result == INTERPRET_OK) {
        printf("\n✓ 数组测试成功\n");
    } else {
        printf("\n✗ 数组测试失败\n");
    }
    
    /* 清理 */
    xr_bc_proto_free(proto);
    xr_ast_free(X, ast);
    xr_bc_vm_free();
    xr_state_free(X);
    
    return (result == INTERPRET_OK) ? 0 : 1;
}


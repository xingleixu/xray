/*
** test_print_bc.c
** 测试字节码VM的print功能
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
    printf("=== Print Function Test ===\n\n");
    
    /* 初始化 */
    XrayState *X = xr_state_new();
    VM vm;

    xr_bc_vm_init(&vm);
    
    /* 测试代码 */
    const char *source = 
        "let x = 42\n"
        "print(x)\n"
        "print(100)\n"
        "print(3.14)\n"
        "let y = x + 8\n"
        "print(y)\n";
    
    printf("源代码:\n%s\n", source);
    
    /* 解析 */
    AstNode *ast = xr_parse(X, source);
    if (ast == NULL) {
        printf("✗ 解析失败\n");
        return 1;
    }
    
    /* 编译 */
    /* 创建编译器上下文 */

    CompilerContext *ctx = xr_compiler_context_new();

    Proto *proto = xr_compile(ctx, ast);

    /* 释放编译器上下文 */

    xr_compiler_context_free(ctx);
    if (proto == NULL) {
        printf("✗ 编译失败\n");
        return 1;
    }
    
    /* 反汇编 */
    printf("\n字节码:\n");
    xr_disassemble_proto(proto, "<test>");
    
    /* 执行 */
    printf("\n执行结果:\n");
    InterpretResult result = xr_bc_interpret_proto(&vm, proto);
    
    if (result == INTERPRET_OK) {
        printf("\n✓ 执行成功\n");
    } else {
        printf("\n✗ 执行失败\n");
    }
    
    /* 清理 */
    xr_bc_proto_free(proto);
    xr_ast_free(X, ast);
    xr_bc_vm_free(&vm);
    xr_state_free(X);
    
    return (result == INTERPRET_OK) ? 0 : 1;
}


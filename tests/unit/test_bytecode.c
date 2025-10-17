/*
** test_bytecode.c
** 字节码VM基础测试
*/

#include "xchunk.h"
#include "xdebug.h"
#include "xvm.h"
#include "xcompiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
** 测试1: 手工构造简单的字节码
** 测试: a = 10 + 20
*/
static void test_simple_arithmetic(void) {
    printf("=== 测试1: 简单算术运算 ===\n");
    
    /* 创建Proto */
    Proto *proto = xr_bc_proto_new();
    proto->name = NULL;
    proto->maxstacksize = 3;
    proto->numparams = 0;
    
    /*
    ** 字节码:
    **   R[0] = 10
    **   R[1] = 20
    **   R[2] = R[0] + R[1]
    **   return R[2]
    */
    xr_bc_proto_write(proto, CREATE_AsBx(OP_LOADI, 0, 10), 1);
    xr_bc_proto_write(proto, CREATE_AsBx(OP_LOADI, 1, 20), 2);
    xr_bc_proto_write(proto, CREATE_ABC(OP_ADD, 2, 0, 1), 3);
    xr_bc_proto_write(proto, CREATE_ABC(OP_RETURN, 2, 1, 0), 4);
    
    /* 反汇编 */
    printf("\n字节码:\n");
    xr_disassemble_proto(proto, "test_arithmetic");
    
    /* 初始化VM */
    VM vm;

    xr_bc_vm_init(&vm);
    
    /* 执行 */
    printf("\n执行结果:\n");
    InterpretResult result = xr_bc_interpret_proto(&vm, proto);
    
    if (result == INTERPRET_OK) {
        printf("✓ 执行成功\n");
    } else {
        printf("✗ 执行失败\n");
    }
    
    /* 清理 */
    xr_bc_vm_free(&vm);
    xr_bc_proto_free(proto);
    
    printf("\n");
}

/*
** 测试2: 测试比较和跳转
** 测试: if (10 > 5) { result = 100; } else { result = 200; }
*/
static void test_conditional(void) {
    printf("=== 测试2: 条件跳转 ===\n");
    
    Proto *proto = xr_bc_proto_new();
    proto->name = NULL;
    proto->maxstacksize = 3;
    proto->numparams = 0;
    
    /*
    ** 字节码:
    **   R[0] = 10
    **   R[1] = 5
    **   if R[0] < R[1] then skip next    ; 比较
    **   jump to else
    **   R[2] = 100                        ; then分支
    **   jump to end
    ** else:
    **   R[2] = 200                        ; else分支
    ** end:
    **   return R[2]
    */
    
    xr_bc_proto_write(proto, CREATE_AsBx(OP_LOADI, 0, 10), 1);
    xr_bc_proto_write(proto, CREATE_AsBx(OP_LOADI, 1, 5), 2);
    xr_bc_proto_write(proto, CREATE_ABC(OP_LT, 0, 1, 0), 3);      /* if 10 < 5 */
    xr_bc_proto_write(proto, CREATE_sJ(OP_JMP, 2), 4);            /* jump +2 (to else) */
    xr_bc_proto_write(proto, CREATE_AsBx(OP_LOADI, 2, 100), 5);   /* then: R[2] = 100 */
    xr_bc_proto_write(proto, CREATE_sJ(OP_JMP, 1), 6);            /* jump +1 (to end) */
    xr_bc_proto_write(proto, CREATE_AsBx(OP_LOADI, 2, 200), 7);   /* else: R[2] = 200 */
    xr_bc_proto_write(proto, CREATE_ABC(OP_RETURN, 2, 1, 0), 8);  /* return R[2] */
    
    /* 反汇编 */
    printf("\n字节码:\n");
    xr_disassemble_proto(proto, "test_conditional");
    
    /* 初始化VM */
    VM vm;

    xr_bc_vm_init(&vm);
    
    /* 执行 */
    printf("\n执行结果:\n");
    InterpretResult result = xr_bc_interpret_proto(&vm, proto);
    
    if (result == INTERPRET_OK) {
        printf("✓ 执行成功（应该返回100，因为10 > 5）\n");
    } else {
        printf("✗ 执行失败\n");
    }
    
    /* 清理 */
    xr_bc_vm_free(&vm);
    xr_bc_proto_free(proto);
    
    printf("\n");
}

/*
** 测试3: 测试常量池
*/
static void test_constants(void) {
    printf("=== 测试3: 常量池 ===\n");
    
    Proto *proto = xr_bc_proto_new();
    proto->name = NULL;
    proto->maxstacksize = 3;
    proto->numparams = 0;
    
    /* 添加常量 */
    int k0 = xr_bc_proto_add_constant(proto, xr_float(3.14159));
    int k1 = xr_bc_proto_add_constant(proto, xr_float(2.71828));
    
    /*
    ** 字节码:
    **   R[0] = K[0]  ; 3.14159
    **   R[1] = K[1]  ; 2.71828
    **   R[2] = R[0] + R[1]
    **   return R[2]
    */
    xr_bc_proto_write(proto, CREATE_ABx(OP_LOADK, 0, k0), 1);
    xr_bc_proto_write(proto, CREATE_ABx(OP_LOADK, 1, k1), 2);
    xr_bc_proto_write(proto, CREATE_ABC(OP_ADD, 2, 0, 1), 3);
    xr_bc_proto_write(proto, CREATE_ABC(OP_RETURN, 2, 1, 0), 4);
    
    /* 反汇编 */
    printf("\n字节码:\n");
    xr_disassemble_proto(proto, "test_constants");
    
    /* 初始化VM */
    VM vm;

    xr_bc_vm_init(&vm);
    
    /* 执行 */
    printf("\n执行结果:\n");
    InterpretResult result = xr_bc_interpret_proto(&vm, proto);
    
    if (result == INTERPRET_OK) {
        printf("✓ 执行成功（应该返回约5.86）\n");
    } else {
        printf("✗ 执行失败\n");
    }
    
    /* 清理 */
    xr_bc_vm_free(&vm);
    xr_bc_proto_free(proto);
    
    printf("\n");
}

/*
** 主函数
*/
int main(void) {
    printf("========================================\n");
    printf("    Xray 字节码VM基础测试\n");
    printf("========================================\n\n");
    
    test_simple_arithmetic();
    test_conditional();
    test_constants();
    
    printf("========================================\n");
    printf("    所有测试完成！\n");
    printf("========================================\n");
    
    return 0;
}


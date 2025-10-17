/* test_structures.c - 闭包数据结构测试 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "xmem.h"
#include "xobject.h"
#include "fn_proto.h"
#include "upvalue.h"
#include "closure.h"

/* 测试辅助宏 */
#define TEST_START(name) \
    printf("测试: %s ... ", name); \
    xmem_init();

#define TEST_END() \
    if (!xmem_check_leaks()) { \
        printf("通过 ✓\n"); \
    } else { \
        printf("失败 ✗ (有内存泄漏)\n"); \
        exit(1); \
    } \
    xmem_cleanup();

/* 测试1: 创建和释放函数原型 */
void test_fn_proto_create() {
    TEST_START("创建和释放函数原型")
    
    XrFnProto *proto = xr_proto_create("testFunc", 2);
    assert(proto != NULL);
    assert(strcmp(proto->name, "testFunc") == 0);
    assert(proto->param_count == 2);
    assert(proto->upval_count == 0);
    
    xr_proto_free(proto);
    
    TEST_END()
}

/* 测试2: 添加 upvalue 描述符 */
void test_fn_proto_add_upvalue() {
    TEST_START("添加 upvalue 描述符")
    
    XrFnProto *proto = xr_proto_create("testFunc", 0);
    
    /* 添加第一个 upvalue */
    int idx1 = xr_proto_add_upvalue(proto, "x", 0, true);
    assert(idx1 == 0);
    assert(proto->upval_count == 1);
    
    /* 添加第二个 upvalue */
    int idx2 = xr_proto_add_upvalue(proto, "y", 1, true);
    assert(idx2 == 1);
    assert(proto->upval_count == 2);
    
    /* 添加重复的 upvalue（应该复用） */
    int idx3 = xr_proto_add_upvalue(proto, "x", 0, true);
    assert(idx3 == 0);  /* 复用第一个 */
    assert(proto->upval_count == 2);  /* 数量不变 */
    
    xr_proto_free(proto);
    
    TEST_END()
}

/* 测试3: 打印函数原型 */
void test_fn_proto_print() {
    printf("测试: 打印函数原型 ... ");
    xmem_init();
    
    XrFnProto *proto = xr_proto_create("myFunc", 3);
    xr_proto_add_upvalue(proto, "x", 0, true);
    xr_proto_add_upvalue(proto, "y", 1, false);
    
    printf("\n");
    xr_proto_print(proto);
    
    xr_proto_free(proto);
    xmem_cleanup();
    
    printf("通过 ✓\n");
}

/* 测试4: 创建和释放 upvalue */
void test_upvalue_create() {
    TEST_START("创建和释放 upvalue")
    
    /* 创建一个栈上的值 */
    XrValue stack_value;
    stack_value.type = XR_TINT;
    stack_value.as.i = 42;
    
    /* 创建 upvalue */
    XrUpvalue *upval = xr_upvalue_create(&stack_value);
    assert(upval != NULL);
    assert(XR_UPVAL_IS_OPEN(upval));
    assert(upval->location == &stack_value);
    assert(upval->location->as.i == 42);
    
    xr_upvalue_free(upval);
    
    TEST_END()
}

/* 测试5: 关闭 upvalue */
void test_upvalue_close() {
    TEST_START("关闭 upvalue")
    
    /* 创建一个栈上的值 */
    XrValue stack_value;
    stack_value.type = XR_TINT;
    stack_value.as.i = 123;
    
    /* 创建 upvalue */
    XrUpvalue *upval = xr_upvalue_create(&stack_value);
    assert(XR_UPVAL_IS_OPEN(upval));
    
    /* 关闭 upvalue */
    xr_upvalue_close(upval);
    assert(XR_UPVAL_IS_CLOSED(upval));
    assert(upval->location == &upval->closed);
    assert(upval->closed.as.i == 123);
    
    /* 修改原来的栈值，upvalue 不应该改变 */
    stack_value.as.i = 999;
    assert(upval->closed.as.i == 123);
    
    xr_upvalue_free(upval);
    
    TEST_END()
}

/* 测试6: 打印 upvalue */
void test_upvalue_print() {
    printf("测试: 打印 upvalue ... ");
    xmem_init();
    
    XrValue val;
    val.type = XR_TINT;
    val.as.i = 55;
    
    XrUpvalue *upval = xr_upvalue_create(&val);
    
    printf("\n开放状态:\n");
    xr_upvalue_print(upval);
    
    xr_upvalue_close(upval);
    printf("关闭状态:\n");
    xr_upvalue_print(upval);
    
    xr_upvalue_free(upval);
    xmem_cleanup();
    
    printf("通过 ✓\n");
}

/* 测试7: 创建和释放闭包 */
void test_closure_create() {
    TEST_START("创建和释放闭包")
    
    /* 创建函数原型 */
    XrFnProto *proto = xr_proto_create("testClosure", 1);
    xr_proto_add_upvalue(proto, "x", 0, true);
    xr_proto_add_upvalue(proto, "y", 1, true);
    
    /* 创建闭包 */
    XrClosure *closure = xr_closure_create(proto);
    assert(closure != NULL);
    assert(closure->proto == proto);
    assert(closure->upvalue_count == 2);
    
    /* 验证 upvalues 数组初始化为 NULL */
    assert(closure->upvalues[0] == NULL);
    assert(closure->upvalues[1] == NULL);
    
    xr_closure_free(closure);
    xr_proto_free(proto);
    
    TEST_END()
}

/* 测试8: 设置闭包的 upvalue */
void test_closure_set_upvalue() {
    TEST_START("设置闭包的 upvalue")
    
    /* 创建函数原型 */
    XrFnProto *proto = xr_proto_create("test", 0);
    xr_proto_add_upvalue(proto, "x", 0, true);
    
    /* 创建闭包 */
    XrClosure *closure = xr_closure_create(proto);
    
    /* 创建 upvalue */
    XrValue val;
    val.type = XR_TINT;
    val.as.i = 100;
    XrUpvalue *upval = xr_upvalue_create(&val);
    
    /* 设置到闭包 */
    xr_closure_set_upvalue(closure, 0, upval);
    assert(closure->upvalues[0] == upval);
    
    /* 通过宏访问 */
    assert(XR_CLOSURE_UPVAL(closure, 0) == upval);
    
    xr_upvalue_free(upval);
    xr_closure_free(closure);
    xr_proto_free(proto);
    
    TEST_END()
}

/* 测试9: 打印闭包 */
void test_closure_print() {
    printf("测试: 打印闭包 ... ");
    xmem_init();
    
    /* 创建函数原型 */
    XrFnProto *proto = xr_proto_create("myClosure", 2);
    xr_proto_add_upvalue(proto, "a", 0, true);
    xr_proto_add_upvalue(proto, "b", 1, false);
    
    /* 创建闭包 */
    XrClosure *closure = xr_closure_create(proto);
    
    /* 创建 upvalues */
    XrValue val1, val2;
    val1.type = XR_TINT;
    val1.as.i = 10;
    val2.type = XR_TINT;
    val2.as.i = 20;
    
    XrUpvalue *uv1 = xr_upvalue_create(&val1);
    XrUpvalue *uv2 = xr_upvalue_create(&val2);
    xr_upvalue_close(uv2);  /* 关闭第二个 */
    
    xr_closure_set_upvalue(closure, 0, uv1);
    xr_closure_set_upvalue(closure, 1, uv2);
    
    printf("\n");
    xr_closure_print(closure);
    
    xr_upvalue_free(uv1);
    xr_upvalue_free(uv2);
    xr_closure_free(closure);
    xr_proto_free(proto);
    xmem_cleanup();
    
    printf("通过 ✓\n");
}

/* 测试10: 内存布局验证 */
void test_memory_layout() {
    TEST_START("内存布局验证")
    
    /* 验证 GC 头包含在对象中 */
    XrFnProto *proto = xr_proto_create("test", 0);
    assert(proto->gc.type == OBJ_FN_PROTO);
    
    XrValue val;
    val.type = XR_TINT;
    val.as.i = 0;
    XrUpvalue *upval = xr_upvalue_create(&val);
    assert(upval->gc.type == OBJ_UPVALUE);
    
    XrClosure *closure = xr_closure_create(proto);
    assert(closure->gc.type == OBJ_CLOSURE);
    
    xr_upvalue_free(upval);
    xr_closure_free(closure);
    xr_proto_free(proto);
    
    TEST_END()
}

int main() {
    printf("=== 闭包数据结构测试 ===\n\n");
    
    test_fn_proto_create();
    test_fn_proto_add_upvalue();
    test_fn_proto_print();
    
    test_upvalue_create();
    test_upvalue_close();
    test_upvalue_print();
    
    test_closure_create();
    test_closure_set_upvalue();
    test_closure_print();
    
    test_memory_layout();
    
    printf("\n所有测试通过！✓\n");
    return 0;
}


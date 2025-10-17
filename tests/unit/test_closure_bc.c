/*
** test_closure_bc.c
** 字节码VM闭包测试
** 
** v0.13.1: 测试闭包的upvalue捕获和访问
*/

#include "xcompiler.h"
#include "xcompiler_context.h"
#include "xvm.h"
#include "xdebug.h"
#include "xparse.h"
#include "xstate.h"
#include "xast.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* 全局状态 */
static XrayState *X = NULL;
static VM vm;  /* VM实例 */

/* 初始化测试环境（只初始化一次） */
static void setup(void) {
    if (X == NULL) {
        X = xr_state_new();
        xr_bc_vm_init(&vm);
    }
}

/* 清理测试环境（不要在每个测试后清理VM） */
static void teardown(void) {
    /* 不清理，让所有测试共享同一个VM */
}

/* 最终清理 */
static void final_teardown(void) {
    if (X != NULL) {
        xr_bc_vm_free(&vm);
        xr_state_free(X);
        X = NULL;
    }
}

/* 编译并执行源代码 */
static InterpretResult run_code(const char *source) {
    /* 语法分析 */
    AstNode *ast = xr_parse(X, source);
    if (ast == NULL) {
        fprintf(stderr, "Parse error\n");
        return INTERPRET_COMPILE_ERROR;
    }
    
    /* 编译 */
    /* 创建编译器上下文 */

    CompilerContext *ctx = xr_compiler_context_new();

    Proto *proto = xr_compile(ctx, ast);

    /* 释放编译器上下文 */

    xr_compiler_context_free(ctx);
    if (proto == NULL) {
        fprintf(stderr, "Compile error\n");
        xr_ast_free(X, ast);
        return INTERPRET_COMPILE_ERROR;
    }
    
    /* 打印字节码（调试用） */
    printf("\n=== Bytecode ===\n");
    xr_disassemble_proto(proto, "script");
    printf("\n");
    
    /* 执行 */
    InterpretResult result = xr_bc_interpret_proto(&vm, proto);
    
    /* 清理 */
    xr_bc_proto_free(proto);
    xr_ast_free(X, ast);
    
    return result;
}

/* ========== 测试用例 ========== */

/*
** 测试1：简单upvalue读取
*/
void test_simple_upvalue_read(void) {
    printf("\n=== Test 1: Simple Upvalue Read ===\n");
    setup();
    
    const char *source = 
        "let x = 10\n"
        "function getX() {\n"
        "    return x\n"
        "}\n"
        "let result = getX()\n";
    
    InterpretResult result = run_code(source);
    assert(result == INTERPRET_OK);
    
    printf("✓ Test 1 passed\n");
    teardown();
}

/*
** 测试2：upvalue写入
*/
void test_upvalue_write(void) {
    printf("\n=== Test 2: Upvalue Write ===\n");
    setup();
    
    const char *source = 
        "let count = 0\n"
        "function increment() {\n"
        "    count = count + 1\n"
        "    return count\n"
        "}\n"
        "let r1 = increment()\n"
        "let r2 = increment()\n";
    
    InterpretResult result = run_code(source);
    assert(result == INTERPRET_OK);
    
    printf("✓ Test 2 passed\n");
    teardown();
}

/*
** 测试3：闭包计数器（经典示例）
*/
void test_counter_closure(void) {
    printf("\n=== Test 3: Counter Closure ===\n");
    setup();
    
    const char *source = 
        "function makeCounter() {\n"
        "    let count = 0\n"
        "    function increment() {\n"
        "        count = count + 1\n"
        "        return count\n"
        "    }\n"
        "    return increment\n"
        "}\n"
        "let counter = makeCounter()\n"
        "let v1 = counter()\n"
        "let v2 = counter()\n";
    
    InterpretResult result = run_code(source);
    assert(result == INTERPRET_OK);
    
    printf("✓ Test 3 passed\n");
    teardown();
}

/*
** 测试4：多个upvalue
*/
void test_multiple_upvalues(void) {
    printf("\n=== Test 4: Multiple Upvalues ===\n");
    setup();
    
    const char *source = 
        "let a = 1\n"
        "let b = 2\n"
        "function add() {\n"
        "    return a + b\n"
        "}\n"
        "let result = add()\n";
    
    InterpretResult result = run_code(source);
    assert(result == INTERPRET_OK);
    
    printf("✓ Test 4 passed\n");
    teardown();
}

/*
** 测试5：嵌套闭包
*/
void test_nested_closures(void) {
    printf("\n=== Test 5: Nested Closures ===\n");
    setup();
    
    const char *source = 
        "function outer(x) {\n"
        "    function middle(y) {\n"
        "        function inner(z) {\n"
        "            return x + y + z\n"
        "        }\n"
        "        return inner\n"
        "    }\n"
        "    return middle\n"
        "}\n"
        "let f1 = outer(1)\n"
        "let f2 = f1(2)\n"
        "let result = f2(3)\n";
    
    InterpretResult result = run_code(source);
    assert(result == INTERPRET_OK);
    
    printf("✓ Test 5 passed\n");
    teardown();
}

/*
** 测试6：upvalue关闭
*/
void test_upvalue_close(void) {
    printf("\n=== Test 6: Upvalue Close ===\n");
    setup();
    
    const char *source = 
        "function makeClosure() {\n"
        "    let x = 10\n"
        "    function getX() {\n"
        "        return x\n"
        "    }\n"
        "    return getX\n"
        "}\n"
        "let getter = makeClosure()\n"
        "let value = getter()\n";
    
    InterpretResult result = run_code(source);
    assert(result == INTERPRET_OK);
    
    printf("✓ Test 6 passed\n");
    teardown();
}

/* ========== 主函数 ========== */

int main(void) {
    printf("====================================\n");
    printf("   Xray Bytecode VM - Closure Tests\n");
    printf("====================================\n");
    
    /* 初始化一次 */
    setup();
    
    test_simple_upvalue_read();
    test_upvalue_write();
    test_counter_closure();
    test_multiple_upvalues();
    test_nested_closures();
    test_upvalue_close();
    
    /* 最终清理 */
    final_teardown();
    
    printf("\n====================================\n");
    printf("   All Closure Tests Passed! ✓\n");
    printf("====================================\n");
    
    return 0;
}


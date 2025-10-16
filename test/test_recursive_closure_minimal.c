/*
** test_recursive_closure_minimal.c
** 最小化递归闭包测试 - 用于定位段错误
*/

#include "../xcompiler.h"
#include "../xvm.h"
#include "../xdebug.h"
#include "../xparse.h"
#include "../xstate.h"
#include "../xast.h"
#include <stdio.h>
#include <assert.h>

static XrayState *X = NULL;

static void setup(void) {
    if (X == NULL) {
        X = xr_state_new();
        xr_bc_vm_init();
    }
}

static void cleanup(void) {
    if (X != NULL) {
        xr_bc_vm_free();
        xr_state_free(X);
        X = NULL;
    }
}

static InterpretResult run_code(const char *source) {
    printf("\n源代码:\n%s\n", source);
    
    AstNode *ast = xr_parse(X, source);
    if (ast == NULL) {
        printf("✗ 解析失败\n");
        return INTERPRET_COMPILE_ERROR;
    }
    
    Proto *proto = xr_compile(ast);
    if (proto == NULL) {
        printf("✗ 编译失败\n");
        xr_ast_free(X, ast);
        return INTERPRET_COMPILE_ERROR;
    }
    
    printf("\n字节码:\n");
    xr_disassemble_proto(proto, "test");
    
    printf("\n执行:\n");
    InterpretResult result = xr_bc_interpret_proto(proto);
    
    xr_bc_proto_free(proto);
    xr_ast_free(X, ast);
    
    return result;
}

/*
** 测试1: 普通递归（无闭包）- 应该工作
*/
void test_simple_recursion(void) {
    printf("\n=== Test 1: Simple Recursion (No Closure) ===\n");
    
    const char *source = 
        "function fib(n) {\n"
        "    if (n <= 1) {\n"
        "        return n\n"
        "    }\n"
        "    return fib(n - 1) + fib(n - 2)\n"
        "}\n"
        "print(fib(5))\n";
    
    InterpretResult result = run_code(source);
    if (result == INTERPRET_OK) {
        printf("✓ Test 1 passed\n");
    } else {
        printf("✗ Test 1 failed\n");
    }
}

/*
** 测试2: 返回递归函数（最简单的递归闭包）
*/
void test_return_recursive_function(void) {
    printf("\n=== Test 2: Return Recursive Function ===\n");
    
    const char *source = 
        "function makeCounter() {\n"
        "    function count(n) {\n"
        "        if (n <= 0) {\n"
        "            return 0\n"
        "        }\n"
        "        return count(n - 1) + 1\n"
        "    }\n"
        "    return count\n"
        "}\n"
        "let c = makeCounter()\n"
        "print(c(3))\n";
    
    InterpretResult result = run_code(source);
    if (result == INTERPRET_OK) {
        printf("✓ Test 2 passed\n");
    } else {
        printf("✗ Test 2 failed (可能段错误)\n");
    }
}

/*
** 测试3: 递归闭包+upvalue
*/
void test_recursive_with_upvalue(void) {
    printf("\n=== Test 3: Recursive with Upvalue ===\n");
    
    const char *source = 
        "function makeSum() {\n"
        "    let total = 0\n"
        "    function sum(n) {\n"
        "        if (n <= 0) {\n"
        "            return total\n"
        "        }\n"
        "        total = total + 1\n"
        "        return sum(n - 1)\n"
        "    }\n"
        "    return sum\n"
        "}\n"
        "let s = makeSum()\n"
        "print(s(5))\n";
    
    InterpretResult result = run_code(source);
    if (result == INTERPRET_OK) {
        printf("✓ Test 3 passed\n");
    } else {
        printf("✗ Test 3 failed\n");
    }
}

/*
** 测试4: 阶乘（原始失败的测试）
*/
void test_factorial_closure(void) {
    printf("\n=== Test 4: Factorial Closure (Original Failing Test) ===\n");
    
    const char *source = 
        "function makeFactorial() {\n"
        "    function factorial(n) {\n"
        "        if (n <= 1) {\n"
        "            return 1\n"
        "        }\n"
        "        return n * factorial(n - 1)\n"
        "    }\n"
        "    return factorial\n"
        "}\n"
        "let fact = makeFactorial()\n"
        "print(fact(5))\n";
    
    InterpretResult result = run_code(source);
    if (result == INTERPRET_OK) {
        printf("✓ Test 4 passed\n");
    } else {
        printf("✗ Test 4 failed\n");
    }
}

int main(void) {
    printf("============================================\n");
    printf("   Recursive Closure Debug Tests\n");
    printf("============================================\n");
    
    setup();
    
    printf("\n开始测试递归闭包...\n");
    printf("注意：如果某个测试导致段错误，后续测试将不会执行\n\n");
    
    test_simple_recursion();
    printf("\n--- 如果到这里都正常，说明普通递归工作 ---\n");
    
    test_return_recursive_function();
    printf("\n--- 如果到这里段错误，问题在返回递归函数 ---\n");
    
    test_recursive_with_upvalue();
    printf("\n--- 如果到这里段错误，问题在递归+upvalue ---\n");
    
    test_factorial_closure();
    printf("\n--- 如果到这里才段错误，问题特定于阶乘 ---\n");
    
    cleanup();
    
    printf("\n============================================\n");
    printf("   如果看到这条消息，所有测试都通过了！\n");
    printf("============================================\n");
    
    return 0;
}


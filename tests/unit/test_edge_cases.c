/*
** test_edge_cases.c
** 边界情况和极端场景测试
** 目标：测试可能被忽略的边界情况
*/

#include "xcompiler.h"
#include "xcompiler_context.h"
#include "xvm.h"
#include "xdebug.h"
#include "xparse.h"
#include "xstate.h"
#include "xast.h"
#include <stdio.h>
#include <assert.h>

static XrayState *X = NULL;
static VM vm;
static int test_count = 0;
static int passed_count = 0;
static int failed_count = 0;

#define TEST_START(name) \
    do { \
        test_count++; \
        printf("\n=== Test %d: %s ===\n", test_count, name); \
    } while(0)

#define TEST_PASS() \
    do { \
        passed_count++; \
        printf("✓ Test %d passed\n", test_count); \
    } while(0)

#define TEST_FAIL(msg) \
    do { \
        failed_count++; \
        printf("✗ Test %d failed: %s\n", test_count, msg); \
    } while(0)

static void setup(void) {
    if (X == NULL) {
        X = xr_state_new();
        xr_bc_vm_init(&vm);
    }
}

static void cleanup(void) {
    if (X != NULL) {
        xr_bc_vm_free(&vm);
        xr_state_free(X);
        X = NULL;
    }
}

static InterpretResult run_code(const char *source) {
    AstNode *ast = xr_parse(X, source);
    if (ast == NULL) {
        return INTERPRET_COMPILE_ERROR;
    }
    
    /* 创建编译器上下文 */

    
    CompilerContext *ctx = xr_compiler_context_new();

    
    Proto *proto = xr_compile(ctx, ast);

    
    /* 释放编译器上下文 */

    
    xr_compiler_context_free(ctx);
    if (proto == NULL) {
        xr_ast_free(X, ast);
        return INTERPRET_COMPILE_ERROR;
    }
    
    InterpretResult result = xr_bc_interpret_proto(&vm, proto);
    
    xr_bc_proto_free(proto);
    xr_ast_free(X, ast);
    
    return result;
}

/* ========== 极端值测试 ========== */

void test_large_integers(void) {
    TEST_START("Large Integers");
    
    const char *source = 
        "let big = 1000000\n"
        "print(big)\n"
        "print(big + 1)\n"
        "print(big * 2)\n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("large integers");
    }
}

void test_small_floats(void) {
    TEST_START("Small Floats");
    
    const char *source = 
        "let small = 0.001\n"
        "print(small)\n"
        "print(small * 1000)\n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("small floats");
    }
}

/* ========== 循环边界测试 ========== */

void test_loop_zero_iterations(void) {
    TEST_START("Loop Zero Iterations");
    
    const char *source = 
        "for (let i = 0; i < 0; i = i + 1) {\n"
        "    print(999)  \n"  // 不应该执行
        "}\n"
        "print(1)  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("loop zero iterations");
    }
}

void test_loop_one_iteration(void) {
    TEST_START("Loop One Iteration");
    
    const char *source = 
        "for (let i = 0; i < 1; i = i + 1) {\n"
        "    print(i)\n"
        "}\n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("loop one iteration");
    }
}

void test_while_false_condition(void) {
    TEST_START("While False Condition");
    
    const char *source = 
        "while (false) {\n"
        "    print(999)  \n"  // 不应该执行
        "}\n"
        "print(1)  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("while false condition");
    }
}

/* ========== 数组边界测试 ========== */

void test_array_empty(void) {
    TEST_START("Array Empty");
    
    const char *source = 
        "let arr = []\n"
        "print(1)  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("array empty");
    }
}

void test_array_single_element(void) {
    TEST_START("Array Single Element");
    
    const char *source = 
        "let arr = [42]\n"
        "print(arr[0])  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("array single element");
    }
}

void test_array_large(void) {
    TEST_START("Array Large");
    
    const char *source = 
        "let arr = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]\n"
        "print(arr[0])  \n"
        "print(arr[5])  \n"
        "print(arr[9])  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("array large");
    }
}

void test_array_sparse(void) {
    TEST_START("Array Sparse");
    
    const char *source = 
        "let arr = []\n"
        "arr[0] = 1\n"
        "arr[5] = 6\n"
        "print(arr[0])  \n"
        "print(arr[5])  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("array sparse");
    }
}

void test_array_three_dimensions(void) {
    TEST_START("Array Three Dimensions");
    
    const char *source = 
        "let cube = [[[1, 2], [3, 4]], [[5, 6], [7, 8]]]\n"
        "print(cube[0][0][0])  \n"
        "print(cube[0][1][1])  \n"
        "print(cube[1][0][0])  \n"
        "print(cube[1][1][1])  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("array three dimensions");
    }
}

/* ========== 函数参数边界测试 ========== */

void test_function_many_parameters(void) {
    TEST_START("Function Many Parameters");
    
    const char *source = 
        "function sum5(a, b, c, d, e) {\n"
        "    return a + b + c + d + e\n"
        "}\n"
        "print(sum5(1, 2, 3, 4, 5))  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("function many parameters");
    }
}

void test_function_return_in_middle(void) {
    TEST_START("Function Return in Middle");
    
    const char *source = 
        "function early(x) {\n"
        "    if (x > 10) {\n"
        "        return 1\n"
        "    }\n"
        "    print(2)\n"
        "    return 3\n"
        "}\n"
        "print(early(20))  \n"
        "print(early(5))   \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("function return in middle");
    }
}

void test_function_multiple_returns(void) {
    TEST_START("Function Multiple Returns");
    
    const char *source = 
        "function classify(x) {\n"
        "    if (x < 0) {\n"
        "        return 1\n"
        "    }\n"
        "    if (x == 0) {\n"
        "        return 2\n"
        "    }\n"
        "    return 3\n"
        "}\n"
        "print(classify(-5))  \n"
        "print(classify(0))   \n"
        "print(classify(5))   \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("function multiple returns");
    }
}

/* ========== 作用域边界测试 ========== */

void test_deeply_nested_scopes(void) {
    TEST_START("Deeply Nested Scopes");
    
    const char *source = 
        "let a = 1\n"
        "{\n"
        "    let b = 2\n"
        "    {\n"
        "        let c = 3\n"
        "        {\n"
        "            let d = 4\n"
        "            print(a + b + c + d)  \n"
        "        }\n"
        "    }\n"
        "}\n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("deeply nested scopes");
    }
}

void test_scope_after_if(void) {
    TEST_START("Scope After If");
    
    const char *source = 
        "let x = 1\n"
        "if (true) {\n"
        "    let y = 2\n"
        "    print(y)\n"
        "}\n"
        "print(x)  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("scope after if");
    }
}

/* ========== 闭包边界测试 ========== */

void test_closure_no_upvalues(void) {
    TEST_START("Closure No Upvalues");
    
    const char *source = 
        "function outer() {\n"
        "    function inner() {\n"
        "        return 42\n"
        "    }\n"
        "    return inner\n"
        "}\n"
        "let f = outer()\n"
        "print(f())  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("closure no upvalues");
    }
}

void test_closure_many_upvalues(void) {
    TEST_START("Closure Many Upvalues");
    
    const char *source = 
        "let a = 1\n"
        "let b = 2\n"
        "let c = 3\n"
        "let d = 4\n"
        "let e = 5\n"
        "function capture() {\n"
        "    return a + b + c + d + e\n"
        "}\n"
        "print(capture())  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("closure many upvalues");
    }
}

void test_closure_modify_all_upvalues(void) {
    TEST_START("Closure Modify All Upvalues");
    
    const char *source = 
        "let x = 1\n"
        "let y = 2\n"
        "let z = 3\n"
        "function modify() {\n"
        "    x = 10\n"
        "    y = 20\n"
        "    z = 30\n"
        "}\n"
        "modify()\n"
        "print(x)  \n"
        "print(y)  \n"
        "print(z)  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("closure modify all upvalues");
    }
}

/* ========== 表达式边界测试 ========== */

void test_expression_all_operators(void) {
    TEST_START("Expression All Operators");
    
    const char *source = 
        "let a = 10\n"
        "let b = 3\n"
        "print(a + b)  \n"
        "print(a - b)  \n"
        "print(a * b)  \n"
        "print(a / b)  \n"
        "print(a % b)  \n"
        "print(-a)     \n"
        "print(!false) \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("expression all operators");
    }
}

void test_expression_precedence(void) {
    TEST_START("Expression Precedence");
    
    const char *source = 
        "print(2 + 3 * 4)      \n"  // 14
        "print((2 + 3) * 4)    \n"  // 20
        "print(10 - 2 - 3)     \n"  // 5
        "print(20 / 2 / 2)     \n"; // 5
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("expression precedence");
    }
}

void test_unary_operators(void) {
    TEST_START("Unary Operators");
    
    const char *source = 
        "let x = 5\n"
        "print(-x)      \n"
        "print(--x)     \n"
        "print(!true)   \n"
        "print(!false)  \n"
        "print(!!true)  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("unary operators");
    }
}

/* ========== 变量作用域极端测试 ========== */

void test_variable_lifecycle(void) {
    TEST_START("Variable Lifecycle");
    
    const char *source = 
        "let x = 1\n"
        "print(x)  \n"
        "{\n"
        "    let x = 2\n"
        "    print(x)  \n"
        "    x = 3\n"
        "    print(x)  \n"
        "}\n"
        "print(x)  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("variable lifecycle");
    }
}

void test_sequential_blocks(void) {
    TEST_START("Sequential Blocks");
    
    const char *source = 
        "{\n"
        "    let x = 1\n"
        "    print(x)\n"
        "}\n"
        "{\n"
        "    let x = 2\n"
        "    print(x)\n"
        "}\n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("sequential blocks");
    }
}

/* ========== 函数调用边界测试 ========== */

void test_function_call_chain(void) {
    TEST_START("Function Call Chain");
    
    const char *source = 
        "function f1(x) { return x + 1 }\n"
        "function f2(x) { return f1(x) + 1 }\n"
        "function f3(x) { return f2(x) + 1 }\n"
        "print(f3(10))  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("function call chain");
    }
}

void test_function_as_expression(void) {
    TEST_START("Function as Expression");
    
    const char *source = 
        "function getValue() {\n"
        "    return 42\n"
        "}\n"
        "let x = getValue() + 10\n"
        "print(x)  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("function as expression");
    }
}

/* ========== 闭包+循环组合测试 ========== */

void test_closure_in_for_loop(void) {
    TEST_START("Closure in For Loop");
    
    const char *source = 
        "function makePrinter(n) {\n"
        "    function print_n() {\n"
        "        return n\n"
        "    }\n"
        "    return print_n\n"
        "}\n"
        "for (let i = 1; i <= 3; i = i + 1) {\n"
        "    let p = makePrinter(i)\n"
        "    print(p())\n"
        "}\n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("closure in for loop");
    }
}

/* ========== 数组+函数组合测试 ========== */

void test_array_function_return(void) {
    TEST_START("Array Function Return");
    
    const char *source = 
        "function makeArray() {\n"
        "    return [1, 2, 3]\n"
        "}\n"
        "let arr = makeArray()\n"
        "print(arr[0])  \n"
        "print(arr[1])  \n"
        "print(arr[2])  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("array function return");
    }
}

void test_function_modify_array_param(void) {
    TEST_START("Function Modify Array Param");
    
    const char *source = 
        "function modify(arr) {\n"
        "    arr[0] = 999\n"
        "}\n"
        "let nums = [1, 2, 3]\n"
        "print(nums[0])  \n"
        "modify(nums)\n"
        "print(nums[0])  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("function modify array param");
    }
}

/* ========== 递归深度测试 ========== */

void test_recursion_depth_100(void) {
    TEST_START("Recursion Depth 100");
    
    const char *source = 
        "function count(n) {\n"
        "    if (n <= 0) {\n"
        "        return 0\n"
        "    }\n"
        "    return count(n - 1) + 1\n"
        "}\n"
        "print(count(100))  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("recursion depth 100");
    }
}

void test_fibonacci_deep(void) {
    TEST_START("Fibonacci Deep");
    
    const char *source = 
        "function fib(n) {\n"
        "    if (n <= 1) {\n"
        "        return n\n"
        "    }\n"
        "    return fib(n - 1) + fib(n - 2)\n"
        "}\n"
        "print(fib(15))  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("fibonacci deep");
    }
}

/* ========== 变量使用边界测试 ========== */

void test_unused_variable(void) {
    TEST_START("Unused Variable");
    
    const char *source = 
        "let x = 10\n"
        "let y = 20\n"
        "print(x)  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("unused variable");
    }
}

void test_variable_used_before_assignment(void) {
    TEST_START("Variable Used Then Assigned");
    
    const char *source = 
        "let x = 10\n"
        "print(x)  \n"
        "x = 20\n"
        "print(x)  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("variable used then assigned");
    }
}

/* ========== 复杂组合测试 ========== */

void test_everything_combined(void) {
    TEST_START("Everything Combined");
    
    const char *source = 
        "let global = 100\n"
        "function process(arr) {\n"
        "    let local = 10\n"
        "    function helper(x) {\n"
        "        return global + local + x\n"
        "    }\n"
        "    for (let i = 0; i < 2; i = i + 1) {\n"
        "        arr[i] = helper(arr[i])\n"
        "    }\n"
        "}\n"
        "let data = [1, 2]\n"
        "print(data[0])  \n"
        "process(data)\n"
        "print(data[0])  \n"
        "print(data[1])  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("everything combined");
    }
}

void test_closure_array_recursion(void) {
    TEST_START("Closure + Array + Recursion");
    
    const char *source = 
        "function makeFibArray() {\n"
        "    let cache = [0, 1]\n"
        "    function fib(n) {\n"
        "        if (n < 2) {\n"
        "            return cache[n]\n"
        "        }\n"
        "        return fib(n - 1) + fib(n - 2)\n"
        "    }\n"
        "    return fib\n"
        "}\n"
        "let f = makeFibArray()\n"
        "print(f(5))  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("closure + array + recursion");
    }
}

/* ========== 主函数 ========== */

int main(void) {
    printf("============================================\n");
    printf("   Xray Edge Cases & Boundary Tests\n");
    printf("============================================\n");
    
    setup();
    
    /* 极端值 */
    test_large_integers();
    test_small_floats();
    
    /* 循环边界 */
    test_loop_zero_iterations();
    test_loop_one_iteration();
    test_while_false_condition();
    
    /* 数组边界 */
    test_array_empty();
    test_array_single_element();
    test_array_large();
    test_array_sparse();
    test_array_three_dimensions();
    
    /* 函数参数 */
    test_function_many_parameters();
    test_function_return_in_middle();
    test_function_multiple_returns();
    
    /* 作用域边界 */
    test_deeply_nested_scopes();
    test_scope_after_if();
    
    /* 闭包边界 */
    test_closure_no_upvalues();
    test_closure_many_upvalues();
    test_closure_modify_all_upvalues();
    
    /* 表达式边界 */
    test_expression_all_operators();
    test_expression_precedence();
    test_unary_operators();
    
    /* 变量使用 */
    test_unused_variable();
    test_variable_used_before_assignment();
    
    /* 复杂组合 */
    test_everything_combined();
    test_closure_array_recursion();
    
    /* 递归深度 */
    test_recursion_depth_100();
    test_fibonacci_deep();
    
    /* 函数调用 */
    test_function_call_chain();
    test_function_as_expression();
    
    /* 作用域 */
    test_sequential_blocks();
    
    cleanup();
    
    printf("\n============================================\n");
    printf("   Test Summary\n");
    printf("============================================\n");
    printf("   Total:  %d tests\n", test_count);
    printf("   Passed: %d tests (%.1f%%)\n", passed_count, 
           test_count > 0 ? (passed_count * 100.0 / test_count) : 0);
    printf("   Failed: %d tests\n", failed_count);
    printf("============================================\n");
    
    if (passed_count == test_count) {
        printf("   🎉 All edge case tests passed!\n");
        return 0;
    } else {
        printf("   ⚠️  Some tests failed\n");
        return 1;
    }
}


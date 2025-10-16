/*
** test_comprehensive_features.c
** Xray语言特性全面测试
** 目标：覆盖所有语言特性，发现潜在bug
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
    AstNode *ast = xr_parse(X, source);
    if (ast == NULL) {
        return INTERPRET_COMPILE_ERROR;
    }
    
    Proto *proto = xr_compile(ast);
    if (proto == NULL) {
        xr_ast_free(X, ast);
        return INTERPRET_COMPILE_ERROR;
    }
    
    InterpretResult result = xr_bc_interpret_proto(proto);
    
    xr_bc_proto_free(proto);
    xr_ast_free(X, ast);
    
    return result;
}

/* ========== 基本类型测试 ========== */

void test_integer_operations(void) {
    TEST_START("Integer Operations");
    
    const char *source = 
        "let a = 42\n"
        "let b = -10\n"
        "print(a + b)  \n"  // 32
        "print(a - b)  \n"  // 52
        "print(a * 2)  \n"  // 84
        "print(a / 2)  \n"  // 21
        "print(a % 5)  \n"; // 2
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("integer operations");
    }
}

void test_float_operations(void) {
    TEST_START("Float Operations");
    
    const char *source = 
        "let x = 3.14\n"
        "let y = 2.0\n"
        "print(x + y)  \n"  // 5.14
        "print(x - y)  \n"  // 1.14
        "print(x * y)  \n"  // 6.28
        "print(x / y)  \n"; // 1.57
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("float operations");
    }
}

void test_boolean_logic(void) {
    TEST_START("Boolean Logic");
    
    const char *source = 
        "let t = true\n"
        "let f = false\n"
        "print(t)      \n"
        "print(f)      \n"
        "print(!t)     \n"
        "print(!f)     \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("boolean logic");
    }
}

void test_null_value(void) {
    TEST_START("Null Value");
    
    const char *source = 
        "let n = null\n"
        "print(n)     \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("null value");
    }
}

/* ========== 比较运算测试 ========== */

void test_comparison_operators(void) {
    TEST_START("Comparison Operators");
    
    const char *source = 
        "let a = 10\n"
        "let b = 20\n"
        "if (a < b) { print(1) }   \n"  // 1
        "if (a > b) { print(2) }   \n"  // 不输出
        "if (a <= b) { print(3) }  \n"  // 3
        "if (a >= b) { print(4) }  \n"  // 不输出
        "if (a == a) { print(5) }  \n"  // 5
        "if (a != b) { print(6) }  \n"; // 6
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("comparison operators");
    }
}

void test_comparison_edge_cases(void) {
    TEST_START("Comparison Edge Cases");
    
    const char *source = 
        "if (0 == 0) { print(1) }       \n"
        "if (1 != 0) { print(2) }       \n"
        "if (-1 < 0) { print(3) }       \n"
        "if (100 > 99) { print(4) }     \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("comparison edge cases");
    }
}

/* ========== 控制流测试 ========== */

void test_if_else_chain(void) {
    TEST_START("If-Else Chain");
    
    const char *source = 
        "let x = 15\n"
        "if (x < 10) {\n"
        "    print(1)\n"
        "} else if (x < 20) {\n"
        "    print(2)\n"
        "} else {\n"
        "    print(3)\n"
        "}\n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("if-else chain");
    }
}

void test_nested_if(void) {
    TEST_START("Nested If");
    
    const char *source = 
        "let x = 10\n"
        "let y = 20\n"
        "if (x > 5) {\n"
        "    if (y > 15) {\n"
        "        print(1)\n"
        "    }\n"
        "}\n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("nested if");
    }
}

void test_while_loop(void) {
    TEST_START("While Loop");
    
    const char *source = 
        "let i = 0\n"
        "while (i < 3) {\n"
        "    print(i)\n"
        "    i = i + 1\n"
        "}\n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("while loop");
    }
}

void test_for_loop(void) {
    TEST_START("For Loop");
    
    const char *source = 
        "for (let i = 0; i < 3; i = i + 1) {\n"
        "    print(i)\n"
        "}\n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("for loop");
    }
}

void test_nested_loops(void) {
    TEST_START("Nested Loops");
    
    const char *source = 
        "for (let i = 0; i < 2; i = i + 1) {\n"
        "    for (let j = 0; j < 2; j = j + 1) {\n"
        "        print(i * 2 + j)\n"
        "    }\n"
        "}\n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("nested loops");
    }
}

/* ========== 函数测试 ========== */

void test_function_basic(void) {
    TEST_START("Function Basic");
    
    const char *source = 
        "function add(a, b) {\n"
        "    return a + b\n"
        "}\n"
        "print(add(10, 20))  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("function basic");
    }
}

void test_function_no_params(void) {
    TEST_START("Function No Parameters");
    
    const char *source = 
        "function getMessage() {\n"
        "    return 42\n"
        "}\n"
        "print(getMessage())  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("function no parameters");
    }
}

void test_function_multiple_params(void) {
    TEST_START("Function Multiple Parameters");
    
    const char *source = 
        "function sum3(a, b, c) {\n"
        "    return a + b + c\n"
        "}\n"
        "print(sum3(1, 2, 3))  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("function multiple parameters");
    }
}

void test_function_recursive(void) {
    TEST_START("Function Recursive");
    
    const char *source = 
        "function countdown(n) {\n"
        "    if (n <= 0) {\n"
        "        return 0\n"
        "    }\n"
        "    print(n)\n"
        "    return countdown(n - 1)\n"
        "}\n"
        "countdown(3)  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("function recursive");
    }
}

void test_function_nested_calls(void) {
    TEST_START("Function Nested Calls");
    
    const char *source = 
        "function double(x) {\n"
        "    return x * 2\n"
        "}\n"
        "function quad(x) {\n"
        "    return double(double(x))\n"
        "}\n"
        "print(quad(5))  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("function nested calls");
    }
}

/* ========== 作用域测试 ========== */

void test_local_scope(void) {
    TEST_START("Local Scope");
    
    const char *source = 
        "let x = 10\n"
        "{\n"
        "    let x = 20\n"
        "    print(x)  \n"  // 20
        "}\n"
        "print(x)      \n"; // 10
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("local scope");
    }
}

void test_scope_shadowing(void) {
    TEST_START("Scope Shadowing");
    
    const char *source = 
        "let x = 1\n"
        "{\n"
        "    let x = 2\n"
        "    {\n"
        "        let x = 3\n"
        "        print(x)  \n"  // 3
        "    }\n"
        "    print(x)      \n"  // 2
        "}\n"
        "print(x)          \n"; // 1
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("scope shadowing");
    }
}

/* ========== 数组测试 ========== */

void test_array_creation(void) {
    TEST_START("Array Creation");
    
    const char *source = 
        "let empty = []\n"
        "let nums = [1, 2, 3]\n"
        "print(nums[0])  \n"
        "print(nums[1])  \n"
        "print(nums[2])  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("array creation");
    }
}

void test_array_assignment(void) {
    TEST_START("Array Assignment");
    
    const char *source = 
        "let arr = [1, 2, 3]\n"
        "arr[0] = 10\n"
        "arr[1] = 20\n"
        "print(arr[0])  \n"
        "print(arr[1])  \n"
        "print(arr[2])  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("array assignment");
    }
}

void test_array_nested(void) {
    TEST_START("Array Nested");
    
    const char *source = 
        "let matrix = [[1, 2], [3, 4]]\n"
        "print(matrix[0][0])  \n"
        "print(matrix[0][1])  \n"
        "print(matrix[1][0])  \n"
        "print(matrix[1][1])  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("array nested");
    }
}

void test_array_in_function(void) {
    TEST_START("Array in Function");
    
    const char *source = 
        "function getFirst(arr) {\n"
        "    return arr[0]\n"
        "}\n"
        "let nums = [100, 200]\n"
        "print(getFirst(nums))  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("array in function");
    }
}

/* ========== 闭包与数组组合测试 ========== */

void test_closure_with_array(void) {
    TEST_START("Closure with Array");
    
    const char *source = 
        "function makeList() {\n"
        "    let list = [1, 2, 3]\n"
        "    function getItem(i) {\n"
        "        return list[i]\n"
        "    }\n"
        "    return getItem\n"
        "}\n"
        "let getter = makeList()\n"
        "print(getter(0))  \n"
        "print(getter(1))  \n"
        "print(getter(2))  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("closure with array");
    }
}

void test_array_of_closures(void) {
    TEST_START("Array of Closures");
    
    const char *source = 
        "function makeAdder(n) {\n"
        "    function add(x) {\n"
        "        return x + n\n"
        "    }\n"
        "    return add\n"
        "}\n"
        "let add5 = makeAdder(5)\n"
        "let add10 = makeAdder(10)\n"
        "print(add5(3))   \n"
        "print(add10(3))  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("array of closures");
    }
}

/* ========== 边界情况测试 ========== */

void test_deep_recursion(void) {
    TEST_START("Deep Recursion");
    
    const char *source = 
        "function sum(n) {\n"
        "    if (n <= 0) {\n"
        "        return 0\n"
        "    }\n"
        "    return n + sum(n - 1)\n"
        "}\n"
        "print(sum(10))  \n"; // 55
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("deep recursion");
    }
}

void test_mutual_recursion(void) {
    TEST_START("Mutual Recursion");
    
    const char *source = 
        "function isEven(n) {\n"
        "    if (n == 0) {\n"
        "        return 1\n"
        "    }\n"
        "    if (n == 1) {\n"
        "        return 0\n"
        "    }\n"
        "    return isOdd(n - 1)\n"
        "}\n"
        "function isOdd(n) {\n"
        "    if (n == 0) {\n"
        "        return 0\n"
        "    }\n"
        "    if (n == 1) {\n"
        "        return 1\n"
        "    }\n"
        "    return isEven(n - 1)\n"
        "}\n"
        "print(isEven(4))  \n"
        "print(isOdd(4))   \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("mutual recursion");
    }
}

void test_zero_handling(void) {
    TEST_START("Zero Handling");
    
    const char *source = 
        "let x = 0\n"
        "print(x)          \n"
        "print(x + 1)      \n"
        "print(x - 1)      \n"
        "print(x * 10)     \n"
        "if (x == 0) { print(1) }  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("zero handling");
    }
}

void test_negative_numbers(void) {
    TEST_START("Negative Numbers");
    
    const char *source = 
        "let x = -5\n"
        "print(x)      \n"
        "print(-x)     \n"
        "print(x + 10) \n"
        "print(x * -2) \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("negative numbers");
    }
}

/* ========== 表达式复杂度测试 ========== */

void test_complex_expression(void) {
    TEST_START("Complex Expression");
    
    const char *source = 
        "let result = (10 + 20) * 3 - 5 / 2\n"
        "print(result)  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("complex expression");
    }
}

void test_nested_expressions(void) {
    TEST_START("Nested Expressions");
    
    const char *source = 
        "let x = ((5 + 3) * 2 + (10 - 4) / 2) * 2\n"
        "print(x)  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("nested expressions");
    }
}

/* ========== 变量赋值测试 ========== */

void test_variable_reassignment(void) {
    TEST_START("Variable Reassignment");
    
    const char *source = 
        "let x = 10\n"
        "print(x)   \n"
        "x = 20\n"
        "print(x)   \n"
        "x = x + 5\n"
        "print(x)   \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("variable reassignment");
    }
}

void test_multiple_assignments(void) {
    TEST_START("Multiple Assignments");
    
    const char *source = 
        "let a = 1\n"
        "let b = 2\n"
        "let c = 3\n"
        "a = b\n"
        "b = c\n"
        "c = a\n"
        "print(a)  \n"
        "print(b)  \n"
        "print(c)  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("multiple assignments");
    }
}

/* ========== 函数与闭包组合测试 ========== */

void test_closure_capturing_loop_variable(void) {
    TEST_START("Closure Capturing Loop Variable");
    
    const char *source = 
        "function makePrinter(val) {\n"
        "    function print_val() {\n"
        "        return val\n"
        "    }\n"
        "    return print_val\n"
        "}\n"
        "let p1 = makePrinter(1)\n"
        "let p2 = makePrinter(2)\n"
        "print(p1())  \n"
        "print(p2())  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("closure capturing loop variable");
    }
}

void test_function_returning_function(void) {
    TEST_START("Function Returning Function");
    
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
        TEST_FAIL("function returning function");
    }
}

/* ========== 边界和错误恢复测试 ========== */

void test_empty_block(void) {
    TEST_START("Empty Block");
    
    const char *source = 
        "{\n"
        "}\n"
        "print(1)  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("empty block");
    }
}

void test_empty_function(void) {
    TEST_START("Empty Function");
    
    const char *source = 
        "function empty() {\n"
        "}\n"
        "empty()\n"
        "print(1)  \n";
    
    if (run_code(source) == INTERPRET_OK) {
        TEST_PASS();
    } else {
        TEST_FAIL("empty function");
    }
}

/* ========== 主函数 ========== */

int main(void) {
    printf("============================================\n");
    printf("   Xray Comprehensive Feature Tests\n");
    printf("============================================\n");
    
    setup();
    
    /* 基本类型 */
    test_integer_operations();
    test_float_operations();
    test_boolean_logic();
    test_null_value();
    
    /* 比较运算 */
    test_comparison_operators();
    test_comparison_edge_cases();
    
    /* 控制流 */
    test_if_else_chain();
    test_nested_if();
    test_while_loop();
    test_for_loop();
    test_nested_loops();
    
    /* 函数 */
    test_function_basic();
    test_function_no_params();
    test_function_multiple_params();
    test_function_recursive();
    test_function_nested_calls();
    
    /* 作用域 */
    test_local_scope();
    test_scope_shadowing();
    
    /* 数组 */
    test_array_creation();
    test_array_assignment();
    test_array_nested();
    test_array_in_function();
    
    /* 闭包与数组 */
    test_closure_with_array();
    test_array_of_closures();
    
    /* 边界情况 */
    test_deep_recursion();
    test_mutual_recursion();
    test_zero_handling();
    test_negative_numbers();
    
    /* 复杂表达式 */
    test_complex_expression();
    test_nested_expressions();
    
    /* 变量赋值 */
    test_variable_reassignment();
    test_multiple_assignments();
    
    /* 函数与闭包组合 */
    test_closure_capturing_loop_variable();
    test_function_returning_function();
    
    /* 边界和错误恢复 */
    test_empty_block();
    test_empty_function();
    
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
        printf("   ✓ All tests passed!\n");
        return 0;
    } else {
        printf("   ✗ Some tests failed\n");
        return 1;
    }
}


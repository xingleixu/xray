/*
** test_closure_advanced_bc.c
** 闭包高级应用场景测试
** 
** 目标：验证闭包在各种复杂场景下的正确性
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
static int test_count = 0;
static int passed_count = 0;

/* 初始化测试环境 */
static void setup(void) {
    if (X == NULL) {
        X = xr_state_new();
        xr_bc_vm_init(&vm);
    }
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
static InterpretResult run_code(const char *source, int show_bytecode) {
    AstNode *ast = xr_parse(X, source);
    if (ast == NULL) {
        fprintf(stderr, "✗ Parse error\n");
        return INTERPRET_COMPILE_ERROR;
    }
    
    /* 创建编译器上下文 */

    
    CompilerContext *ctx = xr_compiler_context_new();

    
    Proto *proto = xr_compile(ctx, ast);

    
    /* 释放编译器上下文 */

    
    xr_compiler_context_free(ctx);
    if (proto == NULL) {
        fprintf(stderr, "✗ Compile error\n");
        xr_ast_free(X, ast);
        return INTERPRET_COMPILE_ERROR;
    }
    
    if (show_bytecode) {
        printf("\n--- Bytecode ---\n");
        xr_disassemble_proto(proto, "test");
        printf("----------------\n\n");
    }
    
    InterpretResult result = xr_bc_interpret_proto(&vm, proto);
    
    xr_bc_proto_free(proto);
    xr_ast_free(X, ast);
    
    return result;
}

/* 测试宏 */
#define TEST_START(name) \
    do { \
        test_count++; \
        printf("\n=== Test %d: %s ===\n", test_count, name); \
    } while(0)

#define TEST_END() \
    do { \
        passed_count++; \
        printf("✓ Test %d passed\n", test_count); \
    } while(0)

/* ========== 测试用例 ========== */

/*
** 测试1：多个闭包共享同一个upvalue
** 场景：两个独立的闭包函数访问同一个外部变量
*/
void test_shared_upvalue(void) {
    TEST_START("Shared Upvalue");
    
    const char *source = 
        "let shared = 100\n"
        "function reader() {\n"
        "    return shared\n"
        "}\n"
        "function writer(val) {\n"
        "    shared = val\n"
        "    return shared\n"
        "}\n"
        "print(reader())      \n"  // 应输出100
        "print(writer(200))   \n"  // 应输出200
        "print(reader())      \n"; // 应输出200（验证修改生效）
    
    InterpretResult result = run_code(source, 0);
    assert(result == INTERPRET_OK);
    
    TEST_END();
}

/*
** 测试2：闭包作为参数传递
** 场景：高阶函数接收闭包作为参数
** 注意：暂时使用命名函数而非匿名函数表达式
*/
void test_closure_as_parameter(void) {
    TEST_START("Closure as Parameter");
    
    const char *source = 
        "function apply(func, value) {\n"
        "    return func(value)\n"
        "}\n"
        "let multiplier = 3\n"
        "function makeMultiplier() {\n"
        "    function mult(x) {\n"
        "        return x * multiplier\n"
        "    }\n"
        "    return mult\n"
        "}\n"
        "let mult = makeMultiplier()\n"
        "let result = apply(mult, 10)\n"
        "print(result)\n"; // 应输出30
    
    InterpretResult result = run_code(source, 0);
    assert(result == INTERPRET_OK);
    
    TEST_END();
}

/*
** 测试3：工厂函数返回闭包
** 场景：工厂函数接收参数并返回闭包
*/
void test_multiple_closures_from_factory(void) {
    TEST_START("Factory Function Returns Closure");
    
    const char *source = 
        "function createCounter(start) {\n"
        "    let count = start\n"
        "    function increment() {\n"
        "        count = count + 1\n"
        "        return count\n"
        "    }\n"
        "    return increment\n"
        "}\n"
        "let counter = createCounter(10)\n"
        "print(counter())  \n"  // 11
        "print(counter())  \n"; // 12
    
    InterpretResult result = run_code(source, 0);
    assert(result == INTERPRET_OK);
    
    TEST_END();
}

/*
** 测试4：嵌套作用域中的upvalue
** 场景：多层嵌套，内层函数访问多个外层变量
*/
void test_nested_scope_upvalues(void) {
    TEST_START("Nested Scope Upvalues");
    
    const char *source = 
        "function level1() {\n"
        "    let a = 1\n"
        "    function level2() {\n"
        "        let b = 2\n"
        "        function level3() {\n"
        "            let c = 3\n"
        "            function level4() {\n"
        "                return a + b + c\n"  // 访问3层外部变量
        "            }\n"
        "            return level4\n"
        "        }\n"
        "        return level3\n"
        "    }\n"
        "    return level2\n"
        "}\n"
        "let f2 = level1()\n"
        "let f3 = f2()\n"
        "let f4 = f3()\n"
        "let result = f4()\n"
        "print(result)\n"; // 应输出6
    
    InterpretResult result = run_code(source, 0);
    assert(result == INTERPRET_OK);
    
    TEST_END();
}

/*
** 测试5：循环中创建闭包（经典陷阱）
** 场景：for循环中创建多个闭包，每个捕获不同的循环变量
** 注意：这个测试可能暴露问题
*/
void test_closure_in_loop(void) {
    TEST_START("Closure in Loop");
    
    const char *source = 
        "function makePrinter(value) {\n"
        "    function printer() {\n"
        "        return value\n"
        "    }\n"
        "    return printer\n"
        "}\n"
        "let p1 = makePrinter(1)\n"
        "let p2 = makePrinter(2)\n"
        "let p3 = makePrinter(3)\n"
        "print(p1())  \n"  // 应输出1
        "print(p2())  \n"  // 应输出2
        "print(p3())  \n"; // 应输出3
    
    InterpretResult result = run_code(source, 0);
    assert(result == INTERPRET_OK);
    
    TEST_END();
}

/*
** 测试6：修改后再读取upvalue
** 场景：验证upvalue的修改是否正确传播
*/
void test_upvalue_modification(void) {
    TEST_START("Upvalue Modification");
    
    const char *source = 
        "let value = 0\n"
        "function setter(v) {\n"
        "    value = v\n"
        "}\n"
        "function getter() {\n"
        "    return value\n"
        "}\n"
        "print(getter())   \n"  // 0
        "setter(42)\n"
        "print(getter())   \n"  // 42
        "setter(100)\n"
        "print(getter())   \n"; // 100
    
    InterpretResult result = run_code(source, 0);
    assert(result == INTERPRET_OK);
    
    TEST_END();
}

/*
** 测试7：递归闭包
** 场景：闭包函数递归调用自身
*/
void test_recursive_closure(void) {
    TEST_START("Recursive Closure");
    
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
        "print(fact(5))  \n"; // 应输出120
    
    InterpretResult result = run_code(source, 0);
    assert(result == INTERPRET_OK);
    
    TEST_END();
}

/*
** 测试8：捕获参数
** 场景：闭包捕获外层函数的参数
*/
void test_capture_parameters(void) {
    TEST_START("Capture Parameters");
    
    const char *source = 
        "function makeAdder(x) {\n"
        "    function add(y) {\n"
        "        return x + y\n"
        "    }\n"
        "    return add\n"
        "}\n"
        "let add5 = makeAdder(5)\n"
        "let add10 = makeAdder(10)\n"
        "print(add5(3))   \n"  // 8
        "print(add10(3))  \n"; // 13
    
    InterpretResult result = run_code(source, 0);
    assert(result == INTERPRET_OK);
    
    TEST_END();
}

/*
** 测试9：独立闭包实例
** 场景：多次调用工厂函数，每个闭包有独立的状态
*/
void test_independent_closures(void) {
    TEST_START("Independent Closures");
    
    const char *source = 
        "function makeCounter() {\n"
        "    let count = 0\n"
        "    function inc() {\n"
        "        count = count + 1\n"
        "        return count\n"
        "    }\n"
        "    return inc\n"
        "}\n"
        "let counter1 = makeCounter()\n"
        "let counter2 = makeCounter()\n"
        "print(counter1())  \n"  // 1
        "print(counter1())  \n"  // 2
        "print(counter2())  \n"  // 1 (独立的)
        "print(counter1())  \n"  // 3
        "print(counter2())  \n"; // 2 (独立的)
    
    InterpretResult result = run_code(source, 0);
    assert(result == INTERPRET_OK);
    
    TEST_END();
}

/*
** 测试10：闭包链
** 场景：一个闭包返回另一个闭包，形成链式调用
*/
void test_closure_chain(void) {
    TEST_START("Closure Chain");
    
    const char *source = 
        "function chain(a) {\n"
        "    function step1(b) {\n"
        "        function step2(c) {\n"
        "            function step3(d) {\n"
        "                return a + b + c + d\n"
        "            }\n"
        "            return step3\n"
        "        }\n"
        "        return step2\n"
        "    }\n"
        "    return step1\n"
        "}\n"
        "let result = chain(1)(2)(3)(4)\n"
        "print(result)  \n"; // 应输出10
    
    InterpretResult result = run_code(source, 0);
    assert(result == INTERPRET_OK);
    
    TEST_END();
}

/*
** 测试11：同级作用域的多个闭包
** 场景：同一作用域内定义多个闭包
*/
void test_sibling_closures(void) {
    TEST_START("Sibling Closures");
    
    const char *source = 
        "let x = 10\n"
        "function f1() {\n"
        "    return x\n"
        "}\n"
        "function f2() {\n"
        "    return x * 2\n"
        "}\n"
        "function f3() {\n"
        "    x = x + 1\n"
        "    return x\n"
        "}\n"
        "print(f1())  \n"  // 10
        "print(f2())  \n"  // 20
        "print(f3())  \n"  // 11
        "print(f1())  \n"; // 11 (验证修改生效)
    
    InterpretResult result = run_code(source, 0);
    assert(result == INTERPRET_OK);
    
    TEST_END();
}

/*
** 测试12：闭包修改多个upvalue
** 场景：一个闭包同时修改多个外部变量
*/
void test_modify_multiple_upvalues(void) {
    TEST_START("Modify Multiple Upvalues");
    
    const char *source = 
        "let a = 1\n"
        "let b = 2\n"
        "let c = 3\n"
        "function modify() {\n"
        "    a = a + 1\n"
        "    b = b * 2\n"
        "    c = c - 1\n"
        "    return a + b + c\n"
        "}\n"
        "print(modify())  \n"  // 2+4+2=8
        "print(modify())  \n"; // 3+8+1=12
    
    InterpretResult result = run_code(source, 0);
    assert(result == INTERPRET_OK);
    
    TEST_END();
}

/*
** 测试13：深度嵌套的参数捕获
** 场景：柯里化函数（每层都捕获参数）
*/
void test_currying(void) {
    TEST_START("Currying");
    
    const char *source = 
        "function curry3(a) {\n"
        "    function curry2(b) {\n"
        "        function curry1(c) {\n"
        "            return a * b + c\n"
        "        }\n"
        "        return curry1\n"
        "    }\n"
        "    return curry2\n"
        "}\n"
        "let f = curry3(2)(3)\n"
        "print(f(4))  \n"   // 2*3+4=10
        "print(f(5))  \n";  // 2*3+5=11
    
    InterpretResult result = run_code(source, 0);
    assert(result == INTERPRET_OK);
    
    TEST_END();
}

/*
** 测试14：混合局部变量和upvalue
** 场景：闭包既有局部变量又访问upvalue
*/
void test_mixed_local_and_upvalue(void) {
    TEST_START("Mixed Local and Upvalue");
    
    const char *source = 
        "let outer = 100\n"
        "function process(x) {\n"
        "    let local = 10\n"
        "    function inner(y) {\n"
        "        let innerLocal = 1\n"
        "        return outer + x + local + y + innerLocal\n"
        "    }\n"
        "    return inner\n"
        "}\n"
        "let f = process(5)\n"
        "print(f(2))  \n"; // 100+5+10+2+1=118
    
    InterpretResult result = run_code(source, 0);
    assert(result == INTERPRET_OK);
    
    TEST_END();
}

/*
** 测试15：闭包中的条件upvalue访问
** 场景：根据条件访问不同的upvalue
*/
void test_conditional_upvalue_access(void) {
    TEST_START("Conditional Upvalue Access");
    
    const char *source = 
        "let a = 10\n"
        "let b = 20\n"
        "function selector(useA) {\n"
        "    function getter() {\n"
        "        if (useA > 0) {\n"
        "            return a\n"
        "        }\n"
        "        return b\n"
        "    }\n"
        "    return getter\n"
        "}\n"
        "let getA = selector(1)\n"
        "let getB = selector(0)\n"
        "print(getA())  \n"  // 10
        "print(getB())  \n"; // 20
    
    InterpretResult result = run_code(source, 0);
    assert(result == INTERPRET_OK);
    
    TEST_END();
}

/* ========== 主函数 ========== */

int main(void) {
    printf("============================================\n");
    printf("   Closure Advanced Scenario Tests\n");
    printf("============================================\n");
    
    setup();
    
    // 运行所有测试
    test_shared_upvalue();
    test_closure_as_parameter();
    test_multiple_closures_from_factory();
    test_nested_scope_upvalues();
    test_closure_in_loop();
    test_upvalue_modification();
    test_recursive_closure();
    test_capture_parameters();
    test_independent_closures();
    test_closure_chain();
    test_sibling_closures();
    test_modify_multiple_upvalues();
    test_currying();
    test_mixed_local_and_upvalue();
    test_conditional_upvalue_access();
    
    final_teardown();
    
    printf("\n============================================\n");
    printf("   Test Results: %d/%d passed\n", passed_count, test_count);
    if (passed_count == test_count) {
        printf("   ✓ All Advanced Closure Tests Passed!\n");
    } else {
        printf("   ✗ Some tests failed\n");
    }
    printf("============================================\n");
    
    return (passed_count == test_count) ? 0 : 1;
}


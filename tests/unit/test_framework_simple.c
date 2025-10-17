/*
** test_framework_simple.c
** 测试框架基础功能演示（无外部依赖）
*/

#include "xtest.h"

/* 简单的被测函数 */
int add(int a, int b) {
    return a + b;
}

int multiply(int a, int b) {
    return a * b;
}

const char* greet(const char *name) {
    static char buffer[100];
    snprintf(buffer, sizeof(buffer), "Hello, %s!", name);
    return buffer;
}

/* 测试主程序 */
XTEST_MAIN_BEGIN("Xray 测试框架 - 基础功能演示")

    /* 测试组1: 基础断言 */
    XTEST_GROUP("基础断言");
    
    XTEST_ASSERT(1 + 1 == 2, "1 + 1 应该等于 2");
    XTEST_ASSERT(true, "true 应该为真");
    XTEST_ASSERT(10 > 5, "10 应该大于 5");
    
    /* 测试组2: 相等断言 */
    XTEST_GROUP("相等性测试");
    
    XTEST_ASSERT_EQ(42, 42, "42 应该等于 42");
    XTEST_ASSERT_EQ(add(10, 20), 30, "10 + 20 = 30");
    XTEST_ASSERT_EQ(multiply(3, 4), 12, "3 * 4 = 12");
    
    /* 测试组3: 不等断言 */
    XTEST_GROUP("不等性测试");
    
    XTEST_ASSERT_NE(10, 20, "10 不应该等于 20");
    XTEST_ASSERT_NE(add(5, 5), 11, "5 + 5 不应该等于 11");
    
    /* 测试组4: 指针断言 */
    XTEST_GROUP("指针测试");
    
    void *null_ptr = NULL;
    int value = 42;
    void *valid_ptr = &value;
    
    XTEST_ASSERT_NULL(null_ptr, "null_ptr 应该为 NULL");
    XTEST_ASSERT_NOT_NULL(valid_ptr, "valid_ptr 不应该为 NULL");
    XTEST_ASSERT_NOT_NULL(&value, "变量地址不应该为 NULL");
    
    /* 测试组5: 字符串断言 */
    XTEST_GROUP("字符串测试");
    
    const char *str1 = "hello";
    const char *str2 = "hello";
    const char *str3 = "world";
    
    XTEST_ASSERT_STR_EQ(str1, str2, "相同的字符串应该相等");
    XTEST_ASSERT_STR_EQ(greet("Alice"), "Hello, Alice!", "greet函数返回正确");
    XTEST_ASSERT(strcmp(str1, str3) != 0, "hello 和 world 不应该相等");
    
    /* 测试组6: 浮点数断言 */
    XTEST_GROUP("浮点数测试");
    
    double pi = 3.14159;
    double approx_pi = 3.14160;
    double bad_pi = 3.5;
    
    XTEST_ASSERT_FLOAT_EQ(pi, approx_pi, 0.001, "π 近似相等（误差 < 0.001）");
    XTEST_ASSERT_FLOAT_EQ(1.0 / 3.0, 0.333333, 0.000001, "1/3 近似等于 0.333333");
    
    /* 这个应该失败（故意的演示） */
    // XTEST_ASSERT_FLOAT_EQ(pi, bad_pi, 0.001, "这个会失败（演示）");
    
    /* 测试组7: 性能测试 */
    XTEST_BENCHMARK("整数加法 100万次", {
        int sum = 0;
        for (int i = 0; i < 1000000; i++) {
            sum += add(i, 1);
        }
    });
    
    XTEST_BENCHMARK("整数乘法 100万次", {
        int product = 1;
        for (int i = 1; i < 100000; i++) {
            product = multiply(product % 1000, 2);
        }
    });

XTEST_MAIN_END()


/*
** test_framework_demo.c
** 演示统一测试框架的使用
*/

#include "xtest.h"
#include "xvalue.h"
#include "xmem.h"

/* 使用新框架的测试示例 */

XTEST_MAIN_BEGIN("Xray 测试框架演示")

    /* 测试组1: 基础断言 */
    XTEST_GROUP("基础断言测试");
    
    XTEST_ASSERT(1 + 1 == 2, "1 + 1 应该等于 2");
    XTEST_ASSERT(true, "true 应该为真");
    XTEST_ASSERT_EQ(42, 42, "42 应该等于 42");
    XTEST_ASSERT_NE(10, 20, "10 不应该等于 20");
    
    /* 测试组2: 指针断言 */
    XTEST_GROUP("指针断言测试");
    
    void *null_ptr = NULL;
    void *valid_ptr = &null_ptr;
    
    XTEST_ASSERT_NULL(null_ptr, "null_ptr 应该为 NULL");
    XTEST_ASSERT_NOT_NULL(valid_ptr, "valid_ptr 不应该为 NULL");
    
    /* 测试组3: 字符串断言 */
    XTEST_GROUP("字符串断言测试");
    
    const char *str1 = "hello";
    const char *str2 = "hello";
    const char *str3 = "world";
    
    XTEST_ASSERT_STR_EQ(str1, str2, "两个hello字符串应该相等");
    XTEST_ASSERT(strcmp(str1, str3) != 0, "hello 和 world 不应该相等");
    
    /* 测试组4: 浮点数断言 */
    XTEST_GROUP("浮点数断言测试");
    
    double pi = 3.14159;
    double approx_pi = 3.14160;
    
    XTEST_ASSERT_FLOAT_EQ(pi, approx_pi, 0.001, "π 近似相等（误差<0.001）");
    
    /* 测试组5: Xray值系统测试 */
    XTEST_GROUP("Xray 值系统测试");
    
    xmem_init();
    
    XrValue v1 = xr_int(42);
    XrValue v2 = xr_float(3.14);
    XrValue v3 = xr_null();
    XrValue v4 = xr_bool(true);
    
    XTEST_ASSERT(xr_isint(v1), "v1 应该是整数");
    XTEST_ASSERT_EQ(42, xr_toint(v1), "v1 的值应该是 42");
    
    XTEST_ASSERT(xr_isfloat(v2), "v2 应该是浮点数");
    XTEST_ASSERT_FLOAT_EQ(3.14, xr_tofloat(v2), 0.001, "v2 的值应该约为 3.14");
    
    XTEST_ASSERT(xr_isnull(v3), "v3 应该是 null");
    XTEST_ASSERT(xr_isbool(v4), "v4 应该是布尔值");
    XTEST_ASSERT(xr_tobool(v4), "v4 的值应该是 true");
    
    xmem_cleanup();
    
    /* 测试组6: 性能测试示例 */
    XTEST_BENCHMARK("创建100万个整数值", {
        for (int i = 0; i < 1000000; i++) {
            XrValue v = xr_int(i);
            (void)v;  /* 避免未使用警告 */
        }
    });

XTEST_MAIN_END()


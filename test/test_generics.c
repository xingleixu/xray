/*
** test_generics.c
** 泛型类型参数测试
*/

#include "../xtype.h"
#include "../xstate.h"
#include "../xvalue.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

/* 测试计数器 */
static int tests_run = 0;
static int tests_passed = 0;

/* 测试宏 */
#define TEST_START(name) \
    printf("测试: %s ... ", name); \
    tests_run++;

#define TEST_PASS() \
    printf("✅ 通过\n"); \
    tests_passed++;

#define TEST_FAIL(msg) \
    printf("❌ 失败: %s\n", msg);

#define ASSERT_NOT_NULL(ptr) \
    if (ptr == NULL) { \
        TEST_FAIL("指针为NULL"); \
        return; \
    }

#define ASSERT_TYPE_KIND(type, expected_kind) \
    if (type == NULL) { \
        TEST_FAIL("类型为NULL"); \
        return; \
    } \
    if (type->kind != expected_kind) { \
        char buf[128]; \
        snprintf(buf, sizeof(buf), "期望类型 %s，实际类型 %s", \
                 xr_type_kind_name(expected_kind), \
                 xr_type_kind_name(type->kind)); \
        TEST_FAIL(buf); \
        return; \
    }

#define ASSERT_EQUALS(a, b) \
    if (a != b) { \
        char buf[128]; \
        snprintf(buf, sizeof(buf), "期望 %d，实际 %d", b, a); \
        TEST_FAIL(buf); \
        return; \
    }

#define ASSERT_STR_EQUALS(a, b) \
    if (strcmp(a, b) != 0) { \
        char buf[128]; \
        snprintf(buf, sizeof(buf), "期望 \"%s\"，实际 \"%s\"", b, a); \
        TEST_FAIL(buf); \
        return; \
    }

/* ========== 类型参数创建测试 ========== */

void test_create_type_param() {
    TEST_START("创建类型参数");
    
    XrayState *X = xray_newstate();
    
    /* 创建类型参数 T */
    XrTypeInfo *t_param = xr_type_param(X, "T", 1);
    
    ASSERT_NOT_NULL(t_param);
    ASSERT_TYPE_KIND(t_param, TYPE_PARAM);
    ASSERT_STR_EQUALS(t_param->as.type_param.name, "T");
    ASSERT_EQUALS(t_param->as.type_param.id, 1);
    
    TEST_PASS();
    
    xr_type_free(t_param);
    // xray_closestate(X); // TODO: 添加状态清理函数
}

void test_create_multiple_type_params() {
    TEST_START("创建多个类型参数");
    
    XrayState *X = xray_newstate();
    
    /* 创建类型参数 T, U, V */
    XrTypeInfo *t = xr_type_param(X, "T", 1);
    XrTypeInfo *u = xr_type_param(X, "U", 2);
    XrTypeInfo *v = xr_type_param(X, "V", 3);
    
    ASSERT_NOT_NULL(t);
    ASSERT_NOT_NULL(u);
    ASSERT_NOT_NULL(v);
    
    ASSERT_STR_EQUALS(t->as.type_param.name, "T");
    ASSERT_STR_EQUALS(u->as.type_param.name, "U");
    ASSERT_STR_EQUALS(v->as.type_param.name, "V");
    
    TEST_PASS();
    
    xr_type_free(t);
    xr_type_free(u);
    xr_type_free(v);
    // xray_closestate(X); // TODO: 添加状态清理函数
}

/* ========== 类型参数映射测试 ========== */

void test_type_param_map() {
    TEST_START("类型参数映射");
    
    XrayState *X = xray_newstate();
    
    /* 创建映射 */
    TypeParamMap *map = xr_type_param_map_new();
    ASSERT_NOT_NULL(map);
    
    /* 添加绑定：T → int */
    xr_type_param_map_add(map, "T", xr_type_int(X));
    
    /* 查找 */
    XrTypeInfo *type = xr_type_param_map_lookup(map, "T");
    ASSERT_NOT_NULL(type);
    ASSERT_TYPE_KIND(type, TYPE_INT);
    
    TEST_PASS();
    
    xr_type_param_map_free(map);
    // xray_closestate(X); // TODO: 添加状态清理函数
}

void test_type_param_map_multiple() {
    TEST_START("多个类型参数映射");
    
    XrayState *X = xray_newstate();
    
    /* 创建映射 */
    TypeParamMap *map = xr_type_param_map_new();
    
    /* 添加多个绑定 */
    xr_type_param_map_add(map, "T", xr_type_int(X));
    xr_type_param_map_add(map, "U", xr_type_string(X));
    xr_type_param_map_add(map, "V", xr_type_float(X));
    
    /* 验证每个绑定 */
    XrTypeInfo *t = xr_type_param_map_lookup(map, "T");
    XrTypeInfo *u = xr_type_param_map_lookup(map, "U");
    XrTypeInfo *v = xr_type_param_map_lookup(map, "V");
    
    ASSERT_TYPE_KIND(t, TYPE_INT);
    ASSERT_TYPE_KIND(u, TYPE_STRING);
    ASSERT_TYPE_KIND(v, TYPE_FLOAT);
    
    TEST_PASS();
    
    xr_type_param_map_free(map);
    // xray_closestate(X); // TODO: 添加状态清理函数
}

/* ========== 类型替换测试 ========== */

void test_substitute_type_param() {
    TEST_START("替换类型参数");
    
    XrayState *X = xray_newstate();
    
    /* 创建类型参数 T */
    XrTypeInfo *t_param = xr_type_param(X, "T", 1);
    
    /* 创建映射：T → int */
    TypeParamMap *map = xr_type_param_map_new();
    xr_type_param_map_add(map, "T", xr_type_int(X));
    
    /* 替换 */
    XrTypeInfo *result = xr_type_substitute(X, t_param, map);
    
    ASSERT_NOT_NULL(result);
    ASSERT_TYPE_KIND(result, TYPE_INT);
    
    TEST_PASS();
    
    xr_type_free(t_param);
    xr_type_param_map_free(map);
    // xray_closestate(X); // TODO: 添加状态清理函数
}

void test_substitute_array_type() {
    TEST_START("替换数组类型中的类型参数");
    
    XrayState *X = xray_newstate();
    
    /* 创建 T[] */
    XrTypeInfo *t_param = xr_type_param(X, "T", 1);
    XrTypeInfo *t_array = xr_type_array(X, t_param);
    
    /* 创建映射：T → string */
    TypeParamMap *map = xr_type_param_map_new();
    xr_type_param_map_add(map, "T", xr_type_string(X));
    
    /* 替换：T[] → string[] */
    XrTypeInfo *result = xr_type_substitute(X, t_array, map);
    
    ASSERT_NOT_NULL(result);
    ASSERT_TYPE_KIND(result, TYPE_ARRAY);
    ASSERT_TYPE_KIND(result->as.array.element_type, TYPE_STRING);
    
    TEST_PASS();
    
    xr_type_param_map_free(map);
    // xray_closestate(X); // TODO: 添加状态清理函数
}

void test_substitute_map_type() {
    TEST_START("替换Map类型中的类型参数");
    
    XrayState *X = xray_newstate();
    
    /* 创建 map<T, U> */
    XrTypeInfo *t_param = xr_type_param(X, "T", 1);
    XrTypeInfo *u_param = xr_type_param(X, "U", 2);
    XrTypeInfo *map_type = xr_type_map(X, t_param, u_param);
    
    /* 创建映射：T → int, U → string */
    TypeParamMap *map = xr_type_param_map_new();
    xr_type_param_map_add(map, "T", xr_type_int(X));
    xr_type_param_map_add(map, "U", xr_type_string(X));
    
    /* 替换：map<T, U> → map<int, string> */
    XrTypeInfo *result = xr_type_substitute(X, map_type, map);
    
    ASSERT_NOT_NULL(result);
    ASSERT_TYPE_KIND(result, TYPE_MAP);
    ASSERT_TYPE_KIND(result->as.map.key_type, TYPE_INT);
    ASSERT_TYPE_KIND(result->as.map.value_type, TYPE_STRING);
    
    TEST_PASS();
    
    xr_type_param_map_free(map);
    // xray_closestate(X); // TODO: 添加状态清理函数
}

void test_substitute_function_type() {
    TEST_START("替换函数类型中的类型参数");
    
    XrayState *X = xray_newstate();
    
    /* 创建 (T, T) => T */
    XrTypeInfo *t_param = xr_type_param(X, "T", 1);
    XrTypeInfo *params[2] = { t_param, t_param };
    XrTypeInfo *func_type = xr_type_function(X, params, 2, t_param);
    
    /* 创建映射：T → float */
    TypeParamMap *map = xr_type_param_map_new();
    xr_type_param_map_add(map, "T", xr_type_float(X));
    
    /* 替换：(T, T) => T → (float, float) => float */
    XrTypeInfo *result = xr_type_substitute(X, func_type, map);
    
    ASSERT_NOT_NULL(result);
    ASSERT_TYPE_KIND(result, TYPE_FUNCTION);
    ASSERT_EQUALS(result->as.function.param_count, 2);
    ASSERT_TYPE_KIND(result->as.function.param_types[0], TYPE_FLOAT);
    ASSERT_TYPE_KIND(result->as.function.param_types[1], TYPE_FLOAT);
    ASSERT_TYPE_KIND(result->as.function.return_type, TYPE_FLOAT);
    
    TEST_PASS();
    
    xr_type_param_map_free(map);
    // xray_closestate(X); // TODO: 添加状态清理函数
}

void test_substitute_nested_type() {
    TEST_START("替换嵌套类型中的类型参数");
    
    XrayState *X = xray_newstate();
    
    /* 创建 T[][] （二维数组）*/
    XrTypeInfo *t_param = xr_type_param(X, "T", 1);
    XrTypeInfo *t_array = xr_type_array(X, t_param);
    XrTypeInfo *t_array2 = xr_type_array(X, t_array);
    
    /* 创建映射：T → int */
    TypeParamMap *map = xr_type_param_map_new();
    xr_type_param_map_add(map, "T", xr_type_int(X));
    
    /* 替换：T[][] → int[][] */
    XrTypeInfo *result = xr_type_substitute(X, t_array2, map);
    
    ASSERT_NOT_NULL(result);
    ASSERT_TYPE_KIND(result, TYPE_ARRAY);
    ASSERT_TYPE_KIND(result->as.array.element_type, TYPE_ARRAY);
    ASSERT_TYPE_KIND(result->as.array.element_type->as.array.element_type, TYPE_INT);
    
    TEST_PASS();
    
    xr_type_param_map_free(map);
    // xray_closestate(X); // TODO: 添加状态清理函数
}

/* ========== 主函数 ========== */

int main() {
    printf("\n========== 泛型类型参数测试 ==========\n\n");
    
    /* 类型参数创建测试 */
    printf("--- 类型参数创建 ---\n");
    test_create_type_param();
    test_create_multiple_type_params();
    
    /* 类型参数映射测试 */
    printf("\n--- 类型参数映射 ---\n");
    test_type_param_map();
    test_type_param_map_multiple();
    
    /* 类型替换测试 */
    printf("\n--- 类型替换 ---\n");
    test_substitute_type_param();
    test_substitute_array_type();
    test_substitute_map_type();
    test_substitute_function_type();
    test_substitute_nested_type();
    
    /* 打印测试结果 */
    printf("\n========== 测试结果 ==========\n");
    printf("总计: %d 个测试\n", tests_run);
    printf("通过: %d 个测试\n", tests_passed);
    printf("失败: %d 个测试\n", tests_run - tests_passed);
    
    if (tests_passed == tests_run) {
        printf("\n✅ 所有测试通过！\n\n");
        return 0;
    } else {
        printf("\n❌ 部分测试失败！\n\n");
        return 1;
    }
}


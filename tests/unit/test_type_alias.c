/*
** test_type_alias.c
** 类型别名测试
*/

#include "xtype.h"
#include "xstate.h"
#include "xvalue.h"
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

#define ASSERT_NULL(ptr) \
    if (ptr != NULL) { \
        TEST_FAIL("指针应为NULL"); \
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

/* ========== 基本类型别名测试 ========== */

void test_register_int_alias() {
    TEST_START("注册int类型别名");
    
    XrayState *X = xr_state_new();
    
    /* 注册类型别名：type UserId = int */
    xr_register_type_alias(X, "UserId", xr_type_int(X));
    
    /* 查找别名 */
    XrTypeInfo *type = xr_resolve_type_alias(X, "UserId");
    
    ASSERT_NOT_NULL(type);
    ASSERT_TYPE_KIND(type, TYPE_INT);
    TEST_PASS();
    
    xr_type_alias_free(X);
    xr_state_free(X);
}

void test_register_string_alias() {
    TEST_START("注册string类型别名");
    
    XrayState *X = xr_state_new();
    
    /* 注册类型别名：type UserName = string */
    xr_register_type_alias(X, "UserName", xr_type_string(X));
    
    /* 查找别名 */
    XrTypeInfo *type = xr_resolve_type_alias(X, "UserName");
    
    ASSERT_NOT_NULL(type);
    ASSERT_TYPE_KIND(type, TYPE_STRING);
    TEST_PASS();
    
    xr_type_alias_free(X);
    xr_state_free(X);
}

void test_register_float_alias() {
    TEST_START("注册float类型别名");
    
    XrayState *X = xr_state_new();
    
    /* 注册类型别名：type Score = float */
    xr_register_type_alias(X, "Score", xr_type_float(X));
    
    /* 查找别名 */
    XrTypeInfo *type = xr_resolve_type_alias(X, "Score");
    
    ASSERT_NOT_NULL(type);
    ASSERT_TYPE_KIND(type, TYPE_FLOAT);
    TEST_PASS();
    
    xr_type_alias_free(X);
    xr_state_free(X);
}

void test_resolve_nonexistent_alias() {
    TEST_START("查找不存在的类型别名");
    
    XrayState *X = xr_state_new();
    
    /* 查找不存在的别名 */
    XrTypeInfo *type = xr_resolve_type_alias(X, "NonExistent");
    
    ASSERT_NULL(type);
    TEST_PASS();
    
    xr_state_free(X);
}

void test_multiple_aliases() {
    TEST_START("注册多个类型别名");
    
    XrayState *X = xr_state_new();
    
    /* 注册多个别名 */
    xr_register_type_alias(X, "UserId", xr_type_int(X));
    xr_register_type_alias(X, "UserName", xr_type_string(X));
    xr_register_type_alias(X, "Score", xr_type_float(X));
    
    /* 验证每个别名 */
    XrTypeInfo *id_type = xr_resolve_type_alias(X, "UserId");
    XrTypeInfo *name_type = xr_resolve_type_alias(X, "UserName");
    XrTypeInfo *score_type = xr_resolve_type_alias(X, "Score");
    
    ASSERT_NOT_NULL(id_type);
    ASSERT_NOT_NULL(name_type);
    ASSERT_NOT_NULL(score_type);
    
    ASSERT_TYPE_KIND(id_type, TYPE_INT);
    ASSERT_TYPE_KIND(name_type, TYPE_STRING);
    ASSERT_TYPE_KIND(score_type, TYPE_FLOAT);
    
    TEST_PASS();
    
    xr_type_alias_free(X);
    xr_state_free(X);
}

void test_update_alias() {
    TEST_START("更新现有类型别名");
    
    XrayState *X = xr_state_new();
    
    /* 注册别名 */
    xr_register_type_alias(X, "MyType", xr_type_int(X));
    
    /* 验证初始类型 */
    XrTypeInfo *type1 = xr_resolve_type_alias(X, "MyType");
    ASSERT_TYPE_KIND(type1, TYPE_INT);
    
    /* 更新别名 */
    xr_register_type_alias(X, "MyType", xr_type_string(X));
    
    /* 验证更新后的类型 */
    XrTypeInfo *type2 = xr_resolve_type_alias(X, "MyType");
    ASSERT_TYPE_KIND(type2, TYPE_STRING);
    
    TEST_PASS();
    
    xr_type_alias_free(X);
    xr_state_free(X);
}

/* ========== 复合类型别名测试 ========== */

void test_array_type_alias() {
    TEST_START("数组类型别名");
    
    XrayState *X = xr_state_new();
    
    /* 注册数组类型别名：type IntArray = int[] */
    XrTypeInfo *int_array = xr_type_array(X, xr_type_int(X));
    xr_register_type_alias(X, "IntArray", int_array);
    
    /* 查找别名 */
    XrTypeInfo *type = xr_resolve_type_alias(X, "IntArray");
    
    ASSERT_NOT_NULL(type);
    ASSERT_TYPE_KIND(type, TYPE_ARRAY);
    ASSERT_TYPE_KIND(type->as.array.element_type, TYPE_INT);
    
    TEST_PASS();
    
    xr_type_alias_free(X);
    xr_state_free(X);
}

void test_map_type_alias() {
    TEST_START("Map类型别名");
    
    XrayState *X = xr_state_new();
    
    /* 注册Map类型别名：type UserMap = map<int, string> */
    XrTypeInfo *user_map = xr_type_map(X, xr_type_int(X), xr_type_string(X));
    xr_register_type_alias(X, "UserMap", user_map);
    
    /* 查找别名 */
    XrTypeInfo *type = xr_resolve_type_alias(X, "UserMap");
    
    ASSERT_NOT_NULL(type);
    ASSERT_TYPE_KIND(type, TYPE_MAP);
    ASSERT_TYPE_KIND(type->as.map.key_type, TYPE_INT);
    ASSERT_TYPE_KIND(type->as.map.value_type, TYPE_STRING);
    
    TEST_PASS();
    
    xr_type_alias_free(X);
    xr_state_free(X);
}

void test_union_type_alias() {
    TEST_START("联合类型别名");
    
    XrayState *X = xr_state_new();
    
    /* 注册联合类型别名：type NumOrStr = int | string */
    XrTypeInfo *types[2] = { xr_type_int(X), xr_type_string(X) };
    XrTypeInfo *union_type = xr_type_union(X, types, 2);
    xr_register_type_alias(X, "NumOrStr", union_type);
    
    /* 查找别名 */
    XrTypeInfo *type = xr_resolve_type_alias(X, "NumOrStr");
    
    ASSERT_NOT_NULL(type);
    ASSERT_TYPE_KIND(type, TYPE_UNION);
    
    TEST_PASS();
    
    xr_type_alias_free(X);
    xr_state_free(X);
}

void test_optional_type_alias() {
    TEST_START("可选类型别名");
    
    XrayState *X = xr_state_new();
    
    /* 注册可选类型别名：type OptionalInt = int? */
    XrTypeInfo *optional_int = xr_type_optional(X, xr_type_int(X));
    xr_register_type_alias(X, "OptionalInt", optional_int);
    
    /* 查找别名 */
    XrTypeInfo *type = xr_resolve_type_alias(X, "OptionalInt");
    
    ASSERT_NOT_NULL(type);
    ASSERT_TYPE_KIND(type, TYPE_UNION);  /* 可选类型是联合类型 */
    
    TEST_PASS();
    
    xr_type_alias_free(X);
    xr_state_free(X);
}

/* ========== 类型别名链测试 ========== */

void test_alias_of_alias() {
    TEST_START("类型别名的别名");
    
    XrayState *X = xr_state_new();
    
    /* 注册别名链：type A = int; type B = A; */
    xr_register_type_alias(X, "A", xr_type_int(X));
    
    XrTypeInfo *type_a = xr_resolve_type_alias(X, "A");
    xr_register_type_alias(X, "B", type_a);
    
    /* 查找B */
    XrTypeInfo *type_b = xr_resolve_type_alias(X, "B");
    
    ASSERT_NOT_NULL(type_b);
    ASSERT_TYPE_KIND(type_b, TYPE_INT);
    
    TEST_PASS();
    
    xr_type_alias_free(X);
    xr_state_free(X);
}

/* ========== 主函数 ========== */

int main() {
    printf("\n========== 类型别名测试 ==========\n\n");
    
    /* 基本类型别名测试 */
    printf("--- 基本类型别名 ---\n");
    test_register_int_alias();
    test_register_string_alias();
    test_register_float_alias();
    test_resolve_nonexistent_alias();
    test_multiple_aliases();
    test_update_alias();
    
    /* 复合类型别名测试 */
    printf("\n--- 复合类型别名 ---\n");
    test_array_type_alias();
    test_map_type_alias();
    test_union_type_alias();
    test_optional_type_alias();
    
    /* 类型别名链测试 */
    printf("\n--- 类型别名链 ---\n");
    test_alias_of_alias();
    
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


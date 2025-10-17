/*
** test_value_type.c
** 测试值对象系统和类型系统
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "xvalue.h"
#include "xtype.h"
#include "xstate.h"

/* 测试计数器 */
static int tests_run = 0;
static int tests_passed = 0;

/* 测试宏 */
#define TEST(name) \
    do { \
        printf("测试: %s ... ", name); \
        tests_run++; \
    } while (0)

#define PASS() \
    do { \
        printf("✓ 通过\n"); \
        tests_passed++; \
    } while (0)

/* ========== 值对象测试 ========== */

void test_value_int() {
    TEST("整数值创建和访问");
    
    XrValue v = xr_int(42);
    assert(xr_isint(v));
    assert(xr_toint(v) == 42);
    
    XrTypeInfo *type = xr_typeof(&v);
    assert(type != NULL);
    assert(type->kind == TYPE_INT);
    
    PASS();
}

void test_value_float() {
    TEST("浮点值创建和访问");
    
    XrValue v = xr_float(3.14);
    assert(xr_isfloat(v));
    assert(xr_tofloat(v) == 3.14);
    
    XrTypeInfo *type = xr_typeof(&v);
    assert(type != NULL);
    assert(type->kind == TYPE_FLOAT);
    
    PASS();
}

void test_value_bool() {
    TEST("布尔值创建和访问");
    
    XrValue v_true = xr_bool(1);
    XrValue v_false = xr_bool(0);
    
    assert(xr_isbool(v_true));
    assert(xr_isbool(v_false));
    assert(xr_tobool(v_true) == 1);
    assert(xr_tobool(v_false) == 0);
    
    PASS();
}

void test_value_null() {
    TEST("null值创建和访问");
    
    XrValue v = xr_null();
    assert(xr_isnull(v));
    
    XrTypeInfo *type = xr_typeof(&v);
    assert(type != NULL);
    assert(type->kind == TYPE_NULL);
    
    PASS();
}

void test_typeof() {
    TEST("xr_typeof函数");
    
    XrValue v_int = xr_int(42);
    XrValue v_float = xr_float(3.14);
    XrValue v_bool = xr_bool(1);
    XrValue v_null = xr_null();
    
    assert(xr_typeof(&v_int)->kind == TYPE_INT);
    assert(xr_typeof(&v_float)->kind == TYPE_FLOAT);
    assert(xr_typeof(&v_bool)->kind == TYPE_BOOL);
    assert(xr_typeof(&v_null)->kind == TYPE_NULL);
    
    PASS();
}

void test_typename_str() {
    TEST("xr_typename_str函数");
    
    XrValue v_int = xr_int(42);
    assert(strcmp(xr_typename_str(&v_int), "int") == 0);
    
    XrValue v_float = xr_float(3.14);
    assert(strcmp(xr_typename_str(&v_float), "float") == 0);
    
    PASS();
}

/* ========== 类型系统测试 ========== */

void test_builtin_types() {
    TEST("内置类型存在");
    
    assert(xr_builtin_int_type.kind == TYPE_INT);
    assert(xr_builtin_float_type.kind == TYPE_FLOAT);
    assert(xr_builtin_string_type.kind == TYPE_STRING);
    assert(xr_builtin_bool_type.kind == TYPE_BOOL);
    assert(xr_builtin_null_type.kind == TYPE_NULL);
    assert(xr_builtin_void_type.kind == TYPE_VOID);
    assert(xr_builtin_any_type.kind == TYPE_ANY);
    
    PASS();
}

void test_type_functions() {
    TEST("类型创建函数");
    
    XrayState X;
    
    XrTypeInfo *t_int = xr_type_int(&X);
    assert(t_int == &xr_builtin_int_type);  /* 返回单例 */
    
    XrTypeInfo *t_float = xr_type_float(&X);
    assert(t_float == &xr_builtin_float_type);
    
    PASS();
}

void test_type_array() {
    TEST("数组类型");
    
    XrayState X;
    
    XrTypeInfo *elem = &xr_builtin_int_type;
    XrTypeInfo *arr = xr_type_array(&X, elem);
    
    assert(arr != NULL);
    assert(arr->kind == TYPE_ARRAY);
    assert(arr->as.array.element_type == elem);
    
    xr_type_free(arr);
    
    PASS();
}

void test_type_union() {
    TEST("联合类型");
    
    XrayState X;
    
    XrTypeInfo *types[2] = {&xr_builtin_int_type, &xr_builtin_string_type};
    XrTypeInfo *union_t = xr_type_union(&X, types, 2);
    
    assert(union_t != NULL);
    assert(union_t->kind == TYPE_UNION);
    assert(union_t->as.union_type.type_count == 2);
    assert(union_t->as.union_type.types[0] == &xr_builtin_int_type);
    assert(union_t->as.union_type.types[1] == &xr_builtin_string_type);
    
    xr_type_free(union_t);
    
    PASS();
}

void test_type_optional() {
    TEST("可选类型");
    
    XrayState X;
    
    XrTypeInfo *opt = xr_type_optional(&X, &xr_builtin_int_type);
    
    assert(opt != NULL);
    assert(opt->kind == TYPE_UNION);  /* int? == int | null */
    assert(opt->as.union_type.type_count == 2);
    
    xr_type_free(opt);
    
    PASS();
}

void test_type_equals() {
    TEST("类型比较");
    
    XrTypeInfo *t1 = &xr_builtin_int_type;
    XrTypeInfo *t2 = &xr_builtin_int_type;
    XrTypeInfo *t3 = &xr_builtin_float_type;
    
    assert(xr_type_equals(t1, t2) == true);
    assert(xr_type_equals(t1, t3) == false);
    
    PASS();
}

void test_type_to_string() {
    TEST("类型转字符串");
    
    char *str_int = xr_type_to_string(&xr_builtin_int_type);
    assert(strcmp(str_int, "int") == 0);
    free(str_int);
    
    XrayState X;
    XrTypeInfo *arr = xr_type_array(&X, &xr_builtin_int_type);
    char *str_arr = xr_type_to_string(arr);
    assert(strcmp(str_arr, "int[]") == 0);
    free(str_arr);
    xr_type_free(arr);
    
    PASS();
}

/* ========== 主函数 ========== */

int main() {
    printf("========================================\n");
    printf("Xray 值对象和类型系统测试\n");
    printf("========================================\n\n");
    
    printf("--- 值对象测试 ---\n");
    test_value_int();
    test_value_float();
    test_value_bool();
    test_value_null();
    test_typeof();
    test_typename_str();
    
    printf("\n--- 类型系统测试 ---\n");
    test_builtin_types();
    test_type_functions();
    test_type_array();
    test_type_union();
    test_type_optional();
    test_type_equals();
    test_type_to_string();
    
    printf("\n========================================\n");
    printf("测试结果:\n");
    printf("  总计: %d\n", tests_run);
    printf("  通过: %d\n", tests_passed);
    printf("  失败: %d\n", tests_run - tests_passed);
    
    if (tests_passed == tests_run) {
        printf("\n✓ 所有测试通过!\n");
    } else {
        printf("\n✗ 有测试失败!\n");
    }
    printf("========================================\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}


/* test_array.c - 数组测试
 *
 * 测试数组的基本功能：
 * - 创建和销毁
 * - push/pop 操作
 * - get/set 访问
 * - unshift/shift
 * - indexOf/contains
 * - 边界检查
 */

#include "xarray.h"
#include "xvalue.h"
#include <stdio.h>
#include <assert.h>

/* 测试计数器 */
static int tests_run = 0;
static int tests_passed = 0;

/* 断言宏 */
#define ASSERT(condition, message) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("✗ 测试失败: %s\n", message); \
        printf("  位置: %s:%d\n", __FILE__, __LINE__); \
    } else { \
        tests_passed++; \
        printf("✓ 通过: %s\n", message); \
    } \
} while(0)

/* ====== 基础测试 ====== */

void test_array_create() {
    printf("\n=== 测试数组创建 ===\n");
    
    // 创建空数组
    XrArray *arr = xr_array_new();
    ASSERT(arr != NULL, "创建空数组");
    ASSERT(arr->count == 0, "空数组长度为0");
    ASSERT(arr->capacity == 0, "空数组容量为0");
    ASSERT(arr->elements == NULL, "空数组元素指针为NULL");
    
    xr_array_free(arr);
    
    // 创建指定容量的数组
    XrArray *arr2 = xr_array_with_capacity(10);
    ASSERT(arr2 != NULL, "创建指定容量数组");
    ASSERT(arr2->count == 0, "初始长度为0");
    ASSERT(arr2->capacity == 10, "初始容量为10");
    ASSERT(arr2->elements != NULL, "元素指针不为NULL");
    
    xr_array_free(arr2);
}

void test_array_push_pop() {
    printf("\n=== 测试 push 和 pop ===\n");
    
    XrArray *arr = xr_array_new();
    
    // push 元素
    xr_array_push(arr, xr_int(10));
    ASSERT(arr->count == 1, "push后长度为1");
    ASSERT(xr_toint(arr->elements[0]) == 10, "第一个元素值正确");
    
    xr_array_push(arr, xr_int(20));
    ASSERT(arr->count == 2, "push后长度为2");
    ASSERT(xr_toint(arr->elements[1]) == 20, "第二个元素值正确");
    
    xr_array_push(arr, xr_int(30));
    ASSERT(arr->count == 3, "push后长度为3");
    
    // pop 元素
    XrValue v1 = xr_array_pop(arr);
    ASSERT(xr_isint(v1) && xr_toint(v1) == 30, "pop返回最后一个元素30");
    ASSERT(arr->count == 2, "pop后长度为2");
    
    XrValue v2 = xr_array_pop(arr);
    ASSERT(xr_isint(v2) && xr_toint(v2) == 20, "pop返回倒数第二个元素20");
    ASSERT(arr->count == 1, "pop后长度为1");
    
    XrValue v3 = xr_array_pop(arr);
    ASSERT(xr_isint(v3) && xr_toint(v3) == 10, "pop返回第一个元素10");
    ASSERT(arr->count == 0, "pop后数组为空");
    
    // 空数组 pop
    XrValue v4 = xr_array_pop(arr);
    ASSERT(xr_isnull(v4), "空数组pop返回null");
    
    xr_array_free(arr);
}

void test_array_get_set() {
    printf("\n=== 测试 get 和 set ===\n");
    
    XrArray *arr = xr_array_new();
    xr_array_push(arr, xr_int(10));
    xr_array_push(arr, xr_int(20));
    xr_array_push(arr, xr_int(30));
    
    // get 测试
    XrValue v0 = xr_array_get(arr, 0);
    ASSERT(xr_isint(v0) && xr_toint(v0) == 10, "get(0)返回10");
    
    XrValue v1 = xr_array_get(arr, 1);
    ASSERT(xr_isint(v1) && xr_toint(v1) == 20, "get(1)返回20");
    
    XrValue v2 = xr_array_get(arr, 2);
    ASSERT(xr_isint(v2) && xr_toint(v2) == 30, "get(2)返回30");
    
    // set 测试
    xr_array_set(arr, 1, xr_int(99));
    XrValue v_new = xr_array_get(arr, 1);
    ASSERT(xr_isint(v_new) && xr_toint(v_new) == 99, "set(1, 99)成功");
    
    // 越界 get
    XrValue v_out = xr_array_get(arr, 10);
    ASSERT(xr_isnull(v_out), "越界get返回null");
    
    // 越界 set（应该不做任何操作）
    xr_array_set(arr, 10, xr_int(100));
    ASSERT(arr->count == 3, "越界set不改变数组长度");
    
    xr_array_free(arr);
}

void test_array_length() {
    printf("\n=== 测试 length ===\n");
    
    XrArray *arr = xr_array_new();
    ASSERT(xr_array_length(arr) == 0, "空数组长度为0");
    
    xr_array_push(arr, xr_int(1));
    ASSERT(xr_array_length(arr) == 1, "添加1个元素后长度为1");
    
    xr_array_push(arr, xr_int(2));
    xr_array_push(arr, xr_int(3));
    ASSERT(xr_array_length(arr) == 3, "添加3个元素后长度为3");
    
    xr_array_pop(arr);
    ASSERT(xr_array_length(arr) == 2, "pop后长度为2");
    
    xr_array_free(arr);
}

void test_array_capacity() {
    printf("\n=== 测试容量管理 ===\n");
    
    XrArray *arr = xr_array_new();
    ASSERT(arr->capacity == 0, "初始容量为0");
    
    // 第一次push，触发扩容到初始容量
    xr_array_push(arr, xr_int(1));
    ASSERT(arr->capacity == XR_ARRAY_INIT_CAPACITY, "第一次扩容到初始容量");
    
    // 填满初始容量
    for (int i = 1; i < XR_ARRAY_INIT_CAPACITY; i++) {
        xr_array_push(arr, xr_int(i));
    }
    ASSERT(arr->count == XR_ARRAY_INIT_CAPACITY, "填满初始容量");
    ASSERT(arr->capacity == XR_ARRAY_INIT_CAPACITY, "容量不变");
    
    // 再push一个，触发翻倍扩容
    xr_array_push(arr, xr_int(99));
    ASSERT(arr->capacity == XR_ARRAY_INIT_CAPACITY * 2, "容量翻倍");
    ASSERT(arr->count == XR_ARRAY_INIT_CAPACITY + 1, "长度正确");
    
    xr_array_free(arr);
}

void test_array_grow() {
    printf("\n=== 测试自动扩容 ===\n");
    
    XrArray *arr = xr_array_new();
    
    // 添加多个元素，测试多次扩容
    for (int i = 0; i < 100; i++) {
        xr_array_push(arr, xr_int(i));
    }
    
    ASSERT(arr->count == 100, "添加100个元素");
    ASSERT(arr->capacity >= 100, "容量足够");
    
    // 验证所有元素正确
    bool all_correct = true;
    for (int i = 0; i < 100; i++) {
        XrValue v = xr_array_get(arr, i);
        if (!xr_isint(v) || xr_toint(v) != i) {
            all_correct = false;
            break;
        }
    }
    ASSERT(all_correct, "所有元素值正确");
    
    xr_array_free(arr);
}

void test_array_bounds() {
    printf("\n=== 测试边界检查 ===\n");
    
    XrArray *arr = xr_array_new();
    xr_array_push(arr, xr_int(10));
    xr_array_push(arr, xr_int(20));
    
    // 负数索引
    XrValue v_neg = xr_array_get(arr, -1);
    ASSERT(xr_isnull(v_neg), "负数索引返回null");
    
    // 超出范围索引
    XrValue v_out = xr_array_get(arr, 100);
    ASSERT(xr_isnull(v_out), "超出范围索引返回null");
    
    // 边界索引
    XrValue v_last = xr_array_get(arr, 1);
    ASSERT(xr_isint(v_last) && xr_toint(v_last) == 20, "最后一个索引正确");
    
    xr_array_free(arr);
}

void test_array_unshift_shift() {
    printf("\n=== 测试 unshift 和 shift ===\n");
    
    XrArray *arr = xr_array_new();
    
    // unshift 测试
    xr_array_unshift(arr, xr_int(3));  // [3]
    ASSERT(arr->count == 1, "unshift后长度为1");
    ASSERT(xr_toint(xr_array_get(arr, 0)) == 3, "unshift的元素在开头");
    
    xr_array_unshift(arr, xr_int(2));  // [2, 3]
    ASSERT(arr->count == 2, "unshift后长度为2");
    ASSERT(xr_toint(xr_array_get(arr, 0)) == 2, "新unshift的元素在开头");
    ASSERT(xr_toint(xr_array_get(arr, 1)) == 3, "原来的元素后移");
    
    xr_array_unshift(arr, xr_int(1));  // [1, 2, 3]
    ASSERT(arr->count == 3, "unshift后长度为3");
    
    // shift 测试
    XrValue v1 = xr_array_shift(arr);  // [2, 3]
    ASSERT(xr_isint(v1) && xr_toint(v1) == 1, "shift返回第一个元素1");
    ASSERT(arr->count == 2, "shift后长度为2");
    ASSERT(xr_toint(xr_array_get(arr, 0)) == 2, "shift后第一个元素正确");
    
    XrValue v2 = xr_array_shift(arr);  // [3]
    ASSERT(xr_isint(v2) && xr_toint(v2) == 2, "shift返回第一个元素2");
    ASSERT(arr->count == 1, "shift后长度为1");
    
    XrValue v3 = xr_array_shift(arr);  // []
    ASSERT(xr_isint(v3) && xr_toint(v3) == 3, "shift返回第一个元素3");
    ASSERT(arr->count == 0, "shift后数组为空");
    
    // 空数组 shift
    XrValue v4 = xr_array_shift(arr);
    ASSERT(xr_isnull(v4), "空数组shift返回null");
    
    xr_array_free(arr);
}

void test_array_contains() {
    printf("\n=== 测试 contains ===\n");
    
    XrArray *arr = xr_array_new();
    xr_array_push(arr, xr_int(10));
    xr_array_push(arr, xr_int(20));
    xr_array_push(arr, xr_int(30));
    
    ASSERT(xr_array_contains(arr, xr_int(10)), "包含10");
    ASSERT(xr_array_contains(arr, xr_int(20)), "包含20");
    ASSERT(xr_array_contains(arr, xr_int(30)), "包含30");
    ASSERT(!xr_array_contains(arr, xr_int(40)), "不包含40");
    ASSERT(!xr_array_contains(arr, xr_int(0)), "不包含0");
    
    xr_array_free(arr);
}

void test_array_index_of() {
    printf("\n=== 测试 indexOf ===\n");
    
    XrArray *arr = xr_array_new();
    xr_array_push(arr, xr_int(10));
    xr_array_push(arr, xr_int(20));
    xr_array_push(arr, xr_int(30));
    xr_array_push(arr, xr_int(20));  // 重复的20
    
    ASSERT(xr_array_index_of(arr, xr_int(10)) == 0, "10的索引是0");
    ASSERT(xr_array_index_of(arr, xr_int(20)) == 1, "20的第一个索引是1");
    ASSERT(xr_array_index_of(arr, xr_int(30)) == 2, "30的索引是2");
    ASSERT(xr_array_index_of(arr, xr_int(40)) == -1, "40不存在，返回-1");
    
    xr_array_free(arr);
}

/* ====== 主测试函数 ====== */

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("   Xray 数组测试 - Day 1 基础功能\n");
    printf("========================================\n");
    
    test_array_create();
    test_array_push_pop();
    test_array_get_set();
    test_array_length();
    test_array_capacity();
    test_array_grow();
    test_array_bounds();
    test_array_unshift_shift();
    test_array_contains();
    test_array_index_of();
    
    printf("\n");
    printf("========================================\n");
    printf("测试完成: %d/%d 通过\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("✓ 所有测试通过！\n");
    } else {
        printf("✗ %d 个测试失败\n", tests_run - tests_passed);
    }
    printf("========================================\n");
    printf("\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}


/* test_map.c - Map字典单元测试
 * 
 * 第11阶段 - Map字典实现
 * 
 * 测试内容：
 *   1. Map创建和销毁
 *   2. set/get/has/delete/clear
 *   3. 哈希冲突处理
 *   4. 动态扩容
 *   5. 墓碑机制
 *   6. 小Map线性扫描优化
 */

#include "../xmap.h"
#include "../xvalue.h"
#include "../xstring.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define TEST(name) \
    printf("测试: %s...", #name); \
    test_##name(); \
    printf(" ✓\n");

#define ASSERT(cond, msg) \
    if (!(cond)) { \
        printf("\n✗ 断言失败: %s\n", msg); \
        printf("  文件: %s:%d\n", __FILE__, __LINE__); \
        exit(1); \
    }

#define ASSERT_EQ(a, b, msg) \
    if ((a) != (b)) { \
        printf("\n✗ 断言失败: %s\n", msg); \
        printf("  期望: %d, 实际: %d\n", (int)(b), (int)(a)); \
        printf("  文件: %s:%d\n", __FILE__, __LINE__); \
        exit(1); \
    }

/* ========== 测试用例 ========== */

/* 测试1: 创建和销毁 */
void test_create_destroy() {
    XrMap *map = xr_map_new();
    ASSERT(map != NULL, "Map创建失败");
    ASSERT_EQ(map->capacity, 0, "初始容量应为0");
    ASSERT_EQ(map->count, 0, "初始数量应为0");
    ASSERT(map->entries == NULL, "初始条目数组应为NULL");
    xr_map_free(map);
}

/* 测试2: 基础插入和查询 */
void test_set_get() {
    XrMap *map = xr_map_new();
    
    /* 插入整数键 */
    XrValue key1 = xr_int(42);
    XrValue val1 = xr_int(100);
    xr_map_set(map, key1, val1);
    
    ASSERT_EQ(xr_map_size(map), 1, "插入后size应为1");
    
    /* 查询 */
    bool found = false;
    XrValue result = xr_map_get(map, key1, &found);
    ASSERT(found, "应该找到键");
    ASSERT(xr_isint(result), "结果应该是整数");
    ASSERT_EQ(xr_toint(result), 100, "值应该是100");
    
    /* 插入字符串键 */
    XrValue key2 = xr_string_value(xr_string_new("hello", 5));
    XrValue val2 = xr_int(200);
    xr_map_set(map, key2, val2);
    
    ASSERT_EQ(xr_map_size(map), 2, "插入后size应为2");
    
    result = xr_map_get(map, key2, &found);
    ASSERT(found, "应该找到字符串键");
    ASSERT_EQ(xr_toint(result), 200, "值应该是200");
    
    xr_map_free(map);
}

/* 测试3: 更新已存在的键 */
void test_update() {
    XrMap *map = xr_map_new();
    
    XrValue key = xr_int(1);
    XrValue val1 = xr_int(100);
    XrValue val2 = xr_int(200);
    
    /* 首次插入 */
    xr_map_set(map, key, val1);
    ASSERT_EQ(xr_map_size(map), 1, "size应为1");
    
    /* 更新 */
    xr_map_set(map, key, val2);
    ASSERT_EQ(xr_map_size(map), 1, "更新后size应仍为1");
    
    /* 验证值已更新 */
    bool found;
    XrValue result = xr_map_get(map, key, &found);
    ASSERT(found, "应该找到键");
    ASSERT_EQ(xr_toint(result), 200, "值应该已更新为200");
    
    xr_map_free(map);
}

/* 测试4: has方法 */
void test_has() {
    XrMap *map = xr_map_new();
    
    XrValue key1 = xr_int(1);
    XrValue key2 = xr_int(2);
    XrValue val = xr_int(100);
    
    /* 插入前 */
    ASSERT(!xr_map_has(map, key1), "不存在的键应返回false");
    
    /* 插入后 */
    xr_map_set(map, key1, val);
    ASSERT(xr_map_has(map, key1), "存在的键应返回true");
    ASSERT(!xr_map_has(map, key2), "不存在的键应返回false");
    
    xr_map_free(map);
}

/* 测试5: delete方法 */
void test_delete() {
    XrMap *map = xr_map_new();
    
    XrValue key1 = xr_int(1);
    XrValue key2 = xr_int(2);
    XrValue key3 = xr_int(3);
    XrValue val = xr_int(100);
    
    /* 插入三个键 */
    xr_map_set(map, key1, val);
    xr_map_set(map, key2, val);
    xr_map_set(map, key3, val);
    ASSERT_EQ(xr_map_size(map), 3, "size应为3");
    
    /* 删除中间的键 */
    bool deleted = xr_map_delete(map, key2);
    ASSERT(deleted, "删除应该成功");
    ASSERT_EQ(xr_map_size(map), 2, "删除后size应为2");
    ASSERT(!xr_map_has(map, key2), "删除的键不应存在");
    
    /* 删除不存在的键 */
    deleted = xr_map_delete(map, key2);
    ASSERT(!deleted, "删除不存在的键应返回false");
    ASSERT_EQ(xr_map_size(map), 2, "size应保持不变");
    
    /* 验证其他键仍然存在 */
    ASSERT(xr_map_has(map, key1), "key1应该仍然存在");
    ASSERT(xr_map_has(map, key3), "key3应该仍然存在");
    
    xr_map_free(map);
}

/* 测试6: clear方法 */
void test_clear() {
    XrMap *map = xr_map_new();
    
    /* 插入多个键值对 */
    for (int i = 0; i < 10; i++) {
        xr_map_set(map, xr_int(i), xr_int(i * 10));
    }
    ASSERT_EQ(xr_map_size(map), 10, "size应为10");
    
    /* 清空 */
    xr_map_clear(map);
    ASSERT_EQ(xr_map_size(map), 0, "清空后size应为0");
    
    /* 验证所有键都不存在 */
    for (int i = 0; i < 10; i++) {
        ASSERT(!xr_map_has(map, xr_int(i)), "清空后键不应存在");
    }
    
    xr_map_free(map);
}

/* 测试7: 动态扩容 */
void test_resize() {
    XrMap *map = xr_map_new();
    
    uint32_t old_capacity = map->capacity;
    
    /* 插入足够多的元素触发扩容 */
    for (int i = 0; i < 100; i++) {
        xr_map_set(map, xr_int(i), xr_int(i * 2));
    }
    
    ASSERT_EQ(xr_map_size(map), 100, "应插入100个元素");
    ASSERT(map->capacity > old_capacity, "容量应该已增长");
    
    /* 验证所有键值对仍然可访问 */
    for (int i = 0; i < 100; i++) {
        bool found;
        XrValue result = xr_map_get(map, xr_int(i), &found);
        ASSERT(found, "扩容后键应仍然存在");
        ASSERT_EQ(xr_toint(result), i * 2, "扩容后值应保持不变");
    }
    
    xr_map_free(map);
}

/* 测试8: 哈希冲突处理 */
void test_collision() {
    XrMap *map = xr_map_new();
    
    /* 插入多个可能冲突的键 */
    for (int i = 0; i < 50; i++) {
        xr_map_set(map, xr_int(i), xr_int(i * 3));
    }
    
    /* 验证所有键都能正确获取 */
    for (int i = 0; i < 50; i++) {
        bool found;
        XrValue result = xr_map_get(map, xr_int(i), &found);
        ASSERT(found, "冲突情况下键应能找到");
        ASSERT_EQ(xr_toint(result), i * 3, "冲突情况下值应正确");
    }
    
    xr_map_free(map);
}

/* 测试9: 小Map线性扫描优化 */
void test_small_map() {
    XrMap *map = xr_map_new();
    
    /* 插入少量元素（≤8） */
    for (int i = 0; i < 8; i++) {
        xr_map_set(map, xr_int(i), xr_int(i * 10));
    }
    
    ASSERT_EQ(xr_map_size(map), 8, "小Map应插入8个元素");
    
    /* 验证查询正确 */
    for (int i = 0; i < 8; i++) {
        bool found;
        XrValue result = xr_map_get(map, xr_int(i), &found);
        ASSERT(found, "小Map应能找到键");
        ASSERT_EQ(xr_toint(result), i * 10, "小Map值应正确");
    }
    
    xr_map_free(map);
}

/* 测试10: 多种键类型 */
void test_multiple_key_types() {
    XrMap *map = xr_map_new();
    
    /* 整数键 */
    xr_map_set(map, xr_int(42), xr_int(1));
    
    /* 字符串键 */
    xr_map_set(map, xr_string_value(xr_string_new("hello", 5)), xr_int(2));
    
    /* 布尔键 */
    xr_map_set(map, xr_bool(1), xr_int(3));
    xr_map_set(map, xr_bool(0), xr_int(4));
    
    /* null键 */
    xr_map_set(map, xr_null(), xr_int(5));
    
    ASSERT_EQ(xr_map_size(map), 5, "应插入5个不同类型的键");
    
    /* 验证所有键 */
    bool found;
    XrValue result;
    
    result = xr_map_get(map, xr_int(42), &found);
    ASSERT(found && xr_toint(result) == 1, "整数键查询");
    
    result = xr_map_get(map, xr_string_value(xr_string_new("hello", 5)), &found);
    ASSERT(found && xr_toint(result) == 2, "字符串键查询");
    
    result = xr_map_get(map, xr_bool(1), &found);
    ASSERT(found && xr_toint(result) == 3, "true键查询");
    
    result = xr_map_get(map, xr_bool(0), &found);
    ASSERT(found && xr_toint(result) == 4, "false键查询");
    
    result = xr_map_get(map, xr_null(), &found);
    ASSERT(found && xr_toint(result) == 5, "null键查询");
    
    xr_map_free(map);
}

/* ========== 主测试函数 ========== */

int main() {
    printf("========== Map字典单元测试 ==========\n\n");
    
    TEST(create_destroy);
    TEST(set_get);
    TEST(update);
    TEST(has);
    TEST(delete);
    TEST(clear);
    TEST(resize);
    TEST(collision);
    TEST(small_map);
    TEST(multiple_key_types);
    
    printf("\n========== 所有测试通过! ==========\n");
    return 0;
}


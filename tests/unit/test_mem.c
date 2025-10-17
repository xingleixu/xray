/* test_mem.c - 内存管理测试 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "xmem.h"
#include "xgc.h"
#include "xobject.h"

/* 测试辅助宏 */
#define TEST_START(name) \
    printf("测试: %s ... ", name); \
    xmem_init();

#define TEST_END() \
    if (!xmem_check_leaks()) { \
        printf("通过 ✓\n"); \
    } else { \
        printf("失败 ✗\n"); \
        exit(1); \
    } \
    xmem_cleanup();

/* 测试1: 基本分配和释放 */
void test_basic_alloc_free() {
    TEST_START("基本分配和释放")
    
    void *p1 = xmem_alloc(100);
    assert(p1 != NULL);
    
    void *p2 = xmem_alloc(200);
    assert(p2 != NULL);
    
    xmem_free(p1);
    xmem_free(p2);
    
    TEST_END()
}

/* 测试2: 内存重新分配 */
void test_realloc() {
    TEST_START("内存重新分配")
    
    void *p = xmem_alloc(100);
    assert(p != NULL);
    
    p = xmem_realloc(p, 100, 200);
    assert(p != NULL);
    
    p = xmem_realloc(p, 200, 50);
    assert(p != NULL);
    
    xmem_free(p);
    
    TEST_END()
}

/* 测试3: 泄漏检测 */
void test_leak_detection() {
    printf("测试: 泄漏检测 ... ");
    xmem_init();
    
    void *p1 = xmem_alloc(100);
    void *p2 = xmem_alloc(200);
    
    xmem_free(p1);
    /* 故意不释放 p2，应该检测到泄漏 */
    
    bool has_leak = xmem_check_leaks();
    assert(has_leak == true);
    
    /* 清理 */
    xmem_free(p2);
    xmem_cleanup();
    
    printf("通过 ✓\n");
}

/* 测试4: 内存统计 */
void test_memory_stats() {
    TEST_START("内存统计")
    
    void *p1 = xmem_alloc(100);
    void *p2 = xmem_alloc(200);
    
    MemoryStats stats = xmem_get_stats();
    assert(stats.current_bytes >= 300);
    assert(stats.peak_bytes >= 300);
    
    xmem_free(p1);
    xmem_free(p2);
    
    TEST_END()
}

/* 测试5: 释放所有内存 */
void test_free_all() {
    TEST_START("释放所有内存")
    
    /* 分配多个对象 */
    for (int i = 0; i < 10; i++) {
        xmem_alloc(100);
    }
    
    /* 一次性释放所有 */
    xmem_free_all();
    
    /* 不应该有泄漏 */
    TEST_END()
}

/* 测试6: GC 头初始化 */
void test_gc_header() {
    TEST_START("GC 头初始化")
    
    GCHeader header;
    gc_init_header(&header, OBJ_STRING);
    
    assert(header.type == OBJ_STRING);
    assert(header.marked == 0);
    assert(header.generation == 0);
    assert(header.next == NULL);
    
    TEST_END()
}

/* 测试7: 统一对象分配 */
void test_unified_allocation() {
    TEST_START("统一对象分配")
    
    /* 定义一个简单的对象类型 */
    typedef struct {
        GCHeader gc;
        int value;
    } TestObj;
    
    /* 使用统一分配宏 */
    TestObj *obj = XR_ALLOCATE(TestObj, OBJ_STRING);
    assert(obj != NULL);
    assert(obj->gc.type == OBJ_STRING);
    
    obj->value = 42;
    assert(obj->value == 42);
    
    xr_free_object(obj);
    
    TEST_END()
}

/* 测试8: 可变长对象分配 */
void test_flex_allocation() {
    TEST_START("可变长对象分配")
    
    /* 定义一个带柔性数组的对象 */
    typedef struct {
        GCHeader gc;
        int count;
        int data[];
    } FlexObj;
    
    /* 分配包含10个int的对象 */
    FlexObj *obj = XR_ALLOCATE_FLEX(FlexObj, int, 10, OBJ_ARRAY);
    assert(obj != NULL);
    assert(obj->gc.type == OBJ_ARRAY);
    
    obj->count = 10;
    for (int i = 0; i < 10; i++) {
        obj->data[i] = i * 2;
    }
    
    /* 验证数据 */
    for (int i = 0; i < 10; i++) {
        assert(obj->data[i] == i * 2);
    }
    
    xr_free_object(obj);
    
    TEST_END()
}

/* 测试9: 压力测试 */
void test_stress() {
    TEST_START("压力测试")
    
    const int COUNT = 1000;
    void *ptrs[COUNT];
    
    /* 分配大量对象 */
    for (int i = 0; i < COUNT; i++) {
        ptrs[i] = xmem_alloc(100);
    }
    
    /* 释放所有对象 */
    for (int i = 0; i < COUNT; i++) {
        xmem_free(ptrs[i]);
    }
    
    TEST_END()
}

/* 测试10: 内存统计打印 */
void test_print_stats() {
    printf("测试: 内存统计打印 ... ");
    xmem_init();
    
    void *p1 = xmem_alloc(100);
    void *p2 = xmem_alloc(200);
    
    xmem_print_stats();
    
    xmem_free(p1);
    xmem_free(p2);
    xmem_cleanup();
    
    printf("通过 ✓\n");
}

int main() {
    printf("=== 内存管理测试 ===\n\n");
    
    test_basic_alloc_free();
    test_realloc();
    test_leak_detection();
    test_memory_stats();
    test_free_all();
    test_gc_header();
    test_unified_allocation();
    test_flex_allocation();
    test_stress();
    test_print_stats();
    
    printf("\n所有测试通过！✓\n");
    return 0;
}


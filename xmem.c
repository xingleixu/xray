/* xmem.c - 简单的内存追踪和管理实现 */

#include "xmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* 全局内存追踪器 */
static MemoryTracker g_tracker = {0};

/* 初始化内存追踪器 */
void xmem_init(void) {
    memset(&g_tracker, 0, sizeof(MemoryTracker));
}

/* 清理内存追踪器 */
void xmem_cleanup(void) {
    xmem_free_all();
    if (g_tracker.allocations) {
        free(g_tracker.allocations);
        g_tracker.allocations = NULL;
    }
    g_tracker.capacity = 0;
}

/* 分配内存（带追踪） */
void* xmem_alloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "Out of memory: failed to allocate %zu bytes\n", size);
        exit(1);
    }
    
    /* 扩容追踪数组（如果需要） */
    if (g_tracker.count >= g_tracker.capacity) {
        size_t new_capacity = g_tracker.capacity == 0 ? 64 : g_tracker.capacity * 2;
        void **new_allocations = (void**)realloc(g_tracker.allocations, 
                                                  new_capacity * sizeof(void*));
        if (!new_allocations) {
            fprintf(stderr, "Out of memory: failed to grow tracker\n");
            free(ptr);
            exit(1);
        }
        g_tracker.allocations = new_allocations;
        g_tracker.capacity = new_capacity;
    }
    
    /* 记录分配 */
    g_tracker.allocations[g_tracker.count++] = ptr;
    g_tracker.total_allocated += size;
    
    /* 更新峰值 */
    size_t current = g_tracker.total_allocated - g_tracker.total_freed;
    if (current > g_tracker.peak_memory) {
        g_tracker.peak_memory = current;
    }
    
    return ptr;
}

/* 重新分配内存（带追踪） */
void* xmem_realloc(void *ptr, size_t old_size, size_t new_size) {
    /* 先释放旧的（从追踪中移除） */
    if (ptr) {
        bool found = false;
        for (size_t i = 0; i < g_tracker.count; i++) {
            if (g_tracker.allocations[i] == ptr) {
                /* 移除这个指针（用最后一个元素替换） */
                g_tracker.allocations[i] = g_tracker.allocations[--g_tracker.count];
                found = true;
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "Warning: reallocating untracked pointer %p\n", ptr);
        }
        g_tracker.total_freed += old_size;
    }
    
    /* 重新分配 */
    void *new_ptr = realloc(ptr, new_size);
    if (!new_ptr && new_size > 0) {
        fprintf(stderr, "Out of memory: failed to reallocate %zu bytes\n", new_size);
        exit(1);
    }
    
    /* 记录新分配 */
    if (new_ptr) {
        /* 扩容追踪数组（如果需要） */
        if (g_tracker.count >= g_tracker.capacity) {
            size_t new_capacity = g_tracker.capacity == 0 ? 64 : g_tracker.capacity * 2;
            void **new_allocations = (void**)realloc(g_tracker.allocations,
                                                      new_capacity * sizeof(void*));
            if (!new_allocations) {
                fprintf(stderr, "Out of memory: failed to grow tracker\n");
                exit(1);
            }
            g_tracker.allocations = new_allocations;
            g_tracker.capacity = new_capacity;
        }
        
        g_tracker.allocations[g_tracker.count++] = new_ptr;
        g_tracker.total_allocated += new_size;
        
        /* 更新峰值 */
        size_t current = g_tracker.total_allocated - g_tracker.total_freed;
        if (current > g_tracker.peak_memory) {
            g_tracker.peak_memory = current;
        }
    }
    
    return new_ptr;
}

/* 释放内存（带追踪） */
void xmem_free(void *ptr) {
    if (!ptr) return;
    
    /* 从追踪器中移除 */
    bool found = false;
    for (size_t i = 0; i < g_tracker.count; i++) {
        if (g_tracker.allocations[i] == ptr) {
            g_tracker.allocations[i] = g_tracker.allocations[--g_tracker.count];
            found = true;
            break;
        }
    }
    
    if (!found) {
        fprintf(stderr, "Warning: attempting to free untracked pointer %p\n", ptr);
    }
    
    free(ptr);
}

/* 释放所有追踪的内存（用于清理） */
void xmem_free_all(void) {
    for (size_t i = 0; i < g_tracker.count; i++) {
        free(g_tracker.allocations[i]);
    }
    g_tracker.count = 0;
}

/* 检查内存泄漏 */
bool xmem_check_leaks(void) {
    if (g_tracker.count > 0) {
        fprintf(stderr, "\n=== 内存泄漏检测 ===\n");
        fprintf(stderr, "泄漏的分配数量: %zu\n", g_tracker.count);
        fprintf(stderr, "当前内存: %zu bytes\n", 
                g_tracker.total_allocated - g_tracker.total_freed);
        return true;
    }
    return false;
}

/* 获取内存统计 */
MemoryStats xmem_get_stats(void) {
    MemoryStats stats;
    stats.current_bytes = g_tracker.total_allocated - g_tracker.total_freed;
    stats.peak_bytes = g_tracker.peak_memory;
    stats.total_allocations = g_tracker.count;
    stats.total_frees = 0; /* 需要额外追踪 */
    return stats;
}

/* 打印内存统计（调试用） */
void xmem_print_stats(void) {
    printf("\n=== 内存统计 ===\n");
    printf("当前分配数量: %zu\n", g_tracker.count);
    printf("当前内存: %zu bytes\n", 
           g_tracker.total_allocated - g_tracker.total_freed);
    printf("峰值内存: %zu bytes\n", g_tracker.peak_memory);
    printf("总分配: %zu bytes\n", g_tracker.total_allocated);
    printf("总释放: %zu bytes\n", g_tracker.total_freed);
}


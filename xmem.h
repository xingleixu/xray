/* xmem.h - 简单的内存追踪和管理
 * 
 * 说明:
 *   - 第8阶段使用简单的内存追踪
 *   - 第14阶段将切换到完整的GC
 *   - 提供内存泄漏检测和统计功能
 */

#ifndef XMEM_H
#define XMEM_H

#include <stddef.h>
#include <stdbool.h>

/* 内存追踪器 */
typedef struct {
    void **allocations;      /* 分配的指针数组 */
    size_t count;           /* 当前分配数量 */
    size_t capacity;        /* 数组容量 */
    size_t total_allocated; /* 总分配字节数 */
    size_t total_freed;     /* 总释放字节数 */
    size_t peak_memory;     /* 峰值内存 */
} MemoryTracker;

/* 内存统计信息 */
typedef struct {
    size_t current_bytes;   /* 当前使用字节数 */
    size_t peak_bytes;      /* 峰值字节数 */
    size_t total_allocations; /* 总分配次数 */
    size_t total_frees;     /* 总释放次数 */
} MemoryStats;

/* 初始化内存追踪器 */
void xmem_init(void);

/* 清理内存追踪器 */
void xmem_cleanup(void);

/* 分配内存（带追踪） */
void* xmem_alloc(size_t size);

/* 重新分配内存（带追踪） */
void* xmem_realloc(void *ptr, size_t old_size, size_t new_size);

/* 释放内存（带追踪） */
void xmem_free(void *ptr);

/* 释放所有追踪的内存（用于清理） */
void xmem_free_all(void);

/* 检查内存泄漏 */
bool xmem_check_leaks(void);

/* 获取内存统计 */
MemoryStats xmem_get_stats(void);

/* 打印内存统计（调试用） */
void xmem_print_stats(void);

/* ========== 便捷宏定义 ========== */

/* 简化的内存分配宏（用于兼容旧代码） */
#define xr_malloc(size) xmem_alloc(size)
#define xr_free(ptr) xmem_free(ptr)
#define xr_realloc(ptr, old_sz, new_sz) xmem_realloc(ptr, old_sz, new_sz)

/* 数组增长宏 */
#define XR_GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

/* 数组增长辅助宏 */
#define XR_GROW_ARRAY(type, pointer, old_count, new_count) \
    (type*)xr_realloc(pointer, sizeof(type) * (old_count), \
                      sizeof(type) * (new_count))

/* 释放数组宏 */
#define XR_FREE_ARRAY(type, pointer, old_count) \
    xr_realloc(pointer, sizeof(type) * (old_count), 0)

#endif /* XMEM_H */


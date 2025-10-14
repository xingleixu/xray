/* xgc.h - GC 接口预留（第14阶段实现）
 * 
 * 说明:
 *   - 第8阶段只定义接口,不实现
 *   - 使用编译开关 XR_USE_GC 控制
 *   - 当前使用 xmem,未来切换到 GC
 *   - 业务代码可以直接使用 gc_alloc 等函数
 */

#ifndef XGC_H
#define XGC_H

#include <stdint.h>
#include <stddef.h>

/* GC 对象头（所有堆对象都包含此头部） */
typedef struct GCHeader {
    uint8_t type;           /* 对象类型 */
    uint8_t marked : 1;     /* GC 标记位（预留） */
    uint8_t generation : 2; /* 分代 GC 代数（预留） */
    uint8_t reserved : 5;   /* 保留位 */
    struct GCHeader *next;  /* GC 对象链表（预留） */
} GCHeader;

/* 对象类型枚举 */
typedef enum {
    OBJ_STRING,
    OBJ_UPVALUE,
    OBJ_CLOSURE,
    OBJ_FN_PROTO,
    OBJ_ARRAY,
    OBJ_MAP,
    /* ... 其他类型 */
} ObjectType;

/* GC 函数声明（第14阶段实现） */
#ifdef XR_USE_GC
    /* GC 分配 */
    void* gc_alloc(size_t size, ObjectType type);
    
    /* GC 重新分配 */
    void* gc_realloc(void *ptr, size_t old_size, size_t new_size);
    
    /* GC 标记对象 */
    void gc_mark_object(void *obj);
    
    /* 执行 GC */
    void gc_collect(void);
    
    /* 临时禁用 GC */
    void gc_pause(void);
    void gc_resume(void);
    
#else
    /* 暂时使用简单内存管理 */
    #include "xmem.h"
    
    #define gc_alloc(size, type)        xmem_alloc(size)
    #define gc_realloc(ptr, old, new)   xmem_realloc(ptr, old, new)
    #define gc_mark_object(obj)         ((void)0)
    #define gc_collect()                ((void)0)
    #define gc_pause()                  ((void)0)
    #define gc_resume()                 ((void)0)
#endif

/* 初始化 GC 头 */
static inline void gc_init_header(GCHeader *header, ObjectType type) {
    header->type = type;
    header->marked = 0;
    header->generation = 0;
    header->reserved = 0;
    header->next = NULL;
}

#endif /* XGC_H */


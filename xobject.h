/* xobject.h - 统一的对象分配接口
 * 
 * 说明:
 *   - 为所有堆对象提供统一的分配接口
 *   - 自动包含GC头
 *   - 支持平滑迁移到GC（业务代码不变）
 */

#ifndef XOBJECT_H
#define XOBJECT_H

#include "xgc.h"

/* 所有堆对象的基类 */
typedef struct {
    GCHeader gc;  /* GC 头（已预留） */
} XObject;

/* 分配对象（统一接口） */
static inline void* allocate_object(size_t size, ObjectType type) {
    void *ptr = gc_alloc(size, type);  /* 现在是 xmem_alloc，未来是 gc_alloc */
    
    /* 初始化 GC 头 */
    gc_init_header((GCHeader*)ptr, type);
    
    return ptr;
}

/* 便捷宏：分配特定类型的对象 */
#define XR_ALLOCATE(type, obj_type) \
    ((type*)allocate_object(sizeof(type), obj_type))

/* 分配可变长对象（带柔性数组成员） */
#define XR_ALLOCATE_FLEX(type, array_type, count, obj_type) \
    ((type*)allocate_object(sizeof(type) + sizeof(array_type) * (count), obj_type))

/* 释放对象（当前手动释放，未来由 GC 管理） */
#ifndef XR_USE_GC
    #define xr_free_object(ptr)  xmem_free(ptr)
#else
    #define xr_free_object(ptr)  ((void)0)  /* GC 自动管理 */
#endif

#endif /* XOBJECT_H */


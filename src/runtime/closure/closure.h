/* closure.h - 闭包对象定义
 * 
 * 说明:
 *   - 闭包 = 函数原型 + 捕获的变量
 *   - 多个闭包可以共享同一个函数原型
 *   - 每个闭包有自己的 upvalue 数组
 */

#ifndef CLOSURE_H
#define CLOSURE_H

#include "xgc.h"
#include "fn_proto.h"
#include "upvalue.h"
#include <stdint.h>
#include <stdbool.h>

/* 闭包 - 函数实例 + 捕获的变量 */
typedef struct XrClosure {
    GCHeader gc;                /* GC 头 */
    
    /* === 核心字段 === */
    XrFnProto *proto;           /* 函数原型（共享） */
    XrUpvalue **upvalues;       /* Upvalue 数组（可变长） */
    uint16_t upvalue_count;     /* Upvalue 数量 */
    
    /* === GC 优化（预留） === */
    uint8_t generation;         /* 分代 GC 代数 */
    bool marked;                /* GC 标记位 */
    
    /* === 优化字段（预留） === */
    uint32_t inline_cache;      /* 内联缓存（预留给 VM 优化） */
    
    /* === 调试信息（可选） === */
#ifdef XR_DEBUG
    uint32_t created_at;        /* 创建时间戳 */
    void *creation_frame;       /* 创建位置（CallFrame*） */
#endif
    
} XrClosure;

/* 访问 upvalue 的便捷宏 */
#define XR_CLOSURE_UPVAL(closure, idx) \
    ((closure)->upvalues[idx])

#define XR_CLOSURE_PROTO(closure) \
    ((closure)->proto)

/* 创建闭包 */
XrClosure* xr_closure_create(XrFnProto *proto);

/* 释放闭包 */
void xr_closure_free(XrClosure *closure);

/* 设置 upvalue */
void xr_closure_set_upvalue(XrClosure *closure, int index, XrUpvalue *upval);

/* 打印闭包信息（调试用） */
void xr_closure_print(XrClosure *closure);

#endif /* CLOSURE_H */


/* upvalue.h - Upvalue（闭包捕获的变量）定义
 * 
 * 说明:
 *   - 基于 Lua 的设计
 *   - 开放/关闭两状态模型
 *   - 开放状态：指向栈上的变量
 *   - 关闭状态：值存储在 upvalue 内部
 */

#ifndef UPVALUE_H
#define UPVALUE_H

#include "xgc.h"
#include "xvalue.h"
#include <stdint.h>
#include <stdbool.h>

/* Upvalue - 闭包捕获的变量（基于 Lua 设计） */
typedef struct XrUpvalue {
    GCHeader gc;                /* GC 头 */
    
    /* === 核心字段 === */
    XrValue *location;          /* 变量位置指针
                                 * - 开放状态：指向栈上的位置
                                 * - 关闭状态：指向 closed 字段
                                 */
    
    XrValue closed;             /* 关闭后的值存储 */
    
    /* === 链表管理（Open upvalue 链表） === */
    struct XrUpvalue *next;     /* 下一个 upvalue */
    
    /* === GC 优化（预留） === */
    uint8_t generation;         /* 分代 GC 代数 */
    bool marked;                /* GC 标记位 */
    
    /* === 调试信息（可选） === */
#ifdef XR_DEBUG
    char *var_name;             /* 变量名 */
    uint32_t capture_line;      /* 捕获位置 */
#endif
    
} XrUpvalue;

/* Upvalue 状态检查宏 */
#define XR_UPVAL_IS_OPEN(uv)   ((uv)->location != &(uv)->closed)
#define XR_UPVAL_IS_CLOSED(uv) ((uv)->location == &(uv)->closed)

/* 创建 upvalue */
XrUpvalue* xr_upvalue_create(XrValue *location);

/* 释放 upvalue */
void xr_upvalue_free(XrUpvalue *upval);

/* 关闭 upvalue */
void xr_upvalue_close(XrUpvalue *upval);

/* 打印 upvalue 信息（调试用） */
void xr_upvalue_print(XrUpvalue *upval);

#endif /* UPVALUE_H */


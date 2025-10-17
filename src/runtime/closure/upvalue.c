/* upvalue.c - Upvalue 实现 */

#include "upvalue.h"
#include "xobject.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 创建 upvalue */
XrUpvalue* xr_upvalue_create(XrValue *location) {
    /* 使用统一对象分配 */
    XrUpvalue *upval = XR_ALLOCATE(XrUpvalue, OBJ_UPVALUE);
    
    /* 初始化字段 */
    upval->location = location;     /* 指向栈上的位置 */
    
    /* 初始化 closed 为 null */
    upval->closed = xr_null();
    
    /* 链表 */
    upval->next = NULL;
    
    /* GC */
    upval->generation = 0;
    upval->marked = false;
    
#ifdef XR_DEBUG
    upval->var_name = NULL;
    upval->capture_line = 0;
#endif
    
    return upval;
}

/* 释放 upvalue */
void xr_upvalue_free(XrUpvalue *upval) {
    if (!upval) return;
    
#ifdef XR_DEBUG
    if (upval->var_name) {
        free(upval->var_name);
    }
#endif
    
    xr_free_object(upval);
}

/* 关闭 upvalue */
void xr_upvalue_close(XrUpvalue *upval) {
    if (!upval) return;
    
    /* 如果已经关闭，直接返回 */
    if (XR_UPVAL_IS_CLOSED(upval)) {
        return;
    }
    
    /* 复制值到 upvalue 内部 */
    upval->closed = *upval->location;
    
    /* 更新指针（指向自己） */
    upval->location = &upval->closed;
}

/* 打印 upvalue 信息（调试用） */
void xr_upvalue_print(XrUpvalue *upval) {
    printf("Upvalue {\n");
    printf("  status: %s\n", XR_UPVAL_IS_OPEN(upval) ? "open" : "closed");
    printf("  location: %p\n", upval->location);
    
    /* 打印值 */
    printf("  value: ");
    XrValue val = *upval->location;
    XrType type = xr_value_type(val);
    switch (type) {
        case XR_TNULL:
            printf("null");
            break;
        case XR_TBOOL:
            printf("%s", xr_tobool(val) ? "true" : "false");
            break;
        case XR_TINT:
            printf("%lld", (long long)xr_toint(val));
            break;
        case XR_TFLOAT:
            printf("%g", xr_tofloat(val));
            break;
        default:
            printf("<object>");
            break;
    }
    printf("\n");
    
#ifdef XR_DEBUG
    if (upval->var_name) {
        printf("  name: %s\n", upval->var_name);
        printf("  captured_at: line %u\n", upval->capture_line);
    }
#endif
    
    printf("}\n");
}


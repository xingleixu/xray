/* closure.c - 闭包对象实现 */

#include "closure.h"
#include "xobject.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 创建闭包 */
XrClosure* xr_closure_create(XrFnProto *proto) {
    if (!proto) {
        return NULL;
    }
    
    /* 使用统一对象分配（包括 upvalue 数组） */
    XrClosure *closure = XR_ALLOCATE_FLEX(
        XrClosure,
        XrUpvalue*,
        proto->upval_count,
        OBJ_CLOSURE
    );
    
    /* 初始化字段 */
    closure->proto = proto;
    closure->upvalue_count = proto->upval_count;
    
    /* Upvalue 数组已在分配时包含，初始化为 NULL */
    closure->upvalues = (XrUpvalue**)(closure + 1);
    for (int i = 0; i < proto->upval_count; i++) {
        closure->upvalues[i] = NULL;
    }
    
    /* GC */
    closure->generation = 0;
    closure->marked = false;
    
    /* 优化 */
    closure->inline_cache = 0;
    
#ifdef XR_DEBUG
    closure->created_at = 0;
    closure->creation_frame = NULL;
#endif
    
    return closure;
}

/* 释放闭包 */
void xr_closure_free(XrClosure *closure) {
    if (!closure) return;
    
    /* 释放 upvalues */
    if (closure->upvalues) {
        for (int i = 0; i < closure->upvalue_count; i++) {
            /* upvalue 由 GC 管理，这里不释放 */
            /* 注意：第14阶段启用 GC 后，这里会自动处理 */
        }
    }
    
    /* 释放对象本身 */
    xr_free_object(closure);
}

/* 设置 upvalue */
void xr_closure_set_upvalue(XrClosure *closure, int index, XrUpvalue *upval) {
    if (!closure || index < 0 || index >= closure->upvalue_count) {
        return;
    }
    
    closure->upvalues[index] = upval;
}

/* 打印闭包信息（调试用） */
void xr_closure_print(XrClosure *closure) {
    printf("Closure {\n");
    printf("  proto: %s\n", 
           closure->proto->name ? closure->proto->name : "<anonymous>");
    printf("  upvalue_count: %d\n", closure->upvalue_count);
    
    if (closure->upvalue_count > 0) {
        printf("  upvalues:\n");
        for (int i = 0; i < closure->upvalue_count; i++) {
            XrUpvalue *uv = closure->upvalues[i];
            if (uv) {
                printf("    [%d] %s\n", i, 
                       XR_UPVAL_IS_OPEN(uv) ? "open" : "closed");
            } else {
                printf("    [%d] <not captured>\n", i);
            }
        }
    }
    
    printf("}\n");
}


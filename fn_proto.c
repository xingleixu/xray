/* fn_proto.c - 函数原型实现 */

#include "fn_proto.h"
#include "xobject.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 创建函数原型 */
XrFnProto* xr_proto_create(const char *name, uint8_t param_count) {
    /* 使用统一对象分配 */
    XrFnProto *proto = XR_ALLOCATE(XrFnProto, OBJ_FN_PROTO);
    
    /* 初始化字段 */
    proto->bytecode = NULL;
    proto->constants = NULL;
    proto->bytecode_len = 0;
    proto->const_count = 0;
    
    /* 函数元信息 */
    if (name) {
        proto->name = strdup(name);
    } else {
        proto->name = NULL;
    }
    proto->param_count = param_count;
    proto->max_stack = 0;
    proto->is_variadic = false;
    
    /* Upvalue 信息 */
    proto->upval_descs = NULL;
    proto->upval_count = 0;
    
    /* 嵌套函数 */
    proto->inner_protos = NULL;
    proto->inner_count = 0;
    
    /* 调试信息 */
    proto->line_info = NULL;
    proto->source_file = NULL;
    
    /* 类型信息 */
    proto->param_types = NULL;
    proto->return_type = NULL;
    
    /* 优化标志 */
    proto->call_count = 0;
    proto->jit_code = NULL;
    proto->optimization_level = 0;
    
    /* AST 节点（当前使用） */
    proto->ast_body = NULL;
    
    return proto;
}

/* 释放函数原型 */
void xr_proto_free(XrFnProto *proto) {
    if (!proto) return;
    
    /* 释放名称 */
    if (proto->name) {
        free(proto->name);
    }
    
    /* 释放字节码 */
    if (proto->bytecode) {
        free(proto->bytecode);
    }
    
    /* 释放常量池 */
    if (proto->constants) {
        free(proto->constants);
    }
    
    /* 释放 upvalue 描述符 */
    if (proto->upval_descs) {
        for (int i = 0; i < proto->upval_count; i++) {
            if (proto->upval_descs[i].name) {
                free(proto->upval_descs[i].name);
            }
        }
        free(proto->upval_descs);
    }
    
    /* 释放内嵌函数原型 */
    if (proto->inner_protos) {
        for (int i = 0; i < proto->inner_count; i++) {
            xr_proto_free(proto->inner_protos[i]);
        }
        free(proto->inner_protos);
    }
    
    /* 释放调试信息 */
    if (proto->line_info) {
        free(proto->line_info);
    }
    if (proto->source_file) {
        free(proto->source_file);
    }
    
    /* 释放对象本身 */
    xr_free_object(proto);
}

/* 添加 upvalue 描述符 */
int xr_proto_add_upvalue(XrFnProto *proto, const char *name, 
                         uint8_t index, bool is_local) {
    /* 检查是否已存在 */
    for (int i = 0; i < proto->upval_count; i++) {
        UpvalDesc *uv = &proto->upval_descs[i];
        if (uv->index == index && uv->is_local == is_local) {
            return i;  /* 复用已有的 */
        }
    }
    
    /* 扩展数组 */
    proto->upval_descs = (UpvalDesc*)realloc(
        proto->upval_descs, 
        (proto->upval_count + 1) * sizeof(UpvalDesc)
    );
    
    /* 添加新的 upvalue */
    UpvalDesc *uv = &proto->upval_descs[proto->upval_count];
    if (name) {
        uv->name = strdup(name);
    } else {
        uv->name = NULL;
    }
    uv->index = index;
    uv->is_local = is_local;
    uv->is_readonly = false;
    
    return proto->upval_count++;
}

/* 打印函数原型信息（调试用） */
void xr_proto_print(XrFnProto *proto) {
    printf("FnProto {\n");
    printf("  name: %s\n", proto->name ? proto->name : "<anonymous>");
    printf("  param_count: %d\n", proto->param_count);
    printf("  upval_count: %d\n", proto->upval_count);
    
    if (proto->upval_count > 0) {
        printf("  upvalues:\n");
        for (int i = 0; i < proto->upval_count; i++) {
            UpvalDesc *uv = &proto->upval_descs[i];
            printf("    [%d] %s (index=%d, %s)\n", 
                   i,
                   uv->name ? uv->name : "<unknown>",
                   uv->index,
                   uv->is_local ? "local" : "upvalue");
        }
    }
    
    printf("}\n");
}


/* fn_proto.h - 函数原型定义
 * 
 * 说明:
 *   - 函数原型(FnProto)是共享的函数定义
 *   - 类似Lua的Proto
 *   - 多个闭包实例可以共享同一个原型
 */

#ifndef FN_PROTO_H
#define FN_PROTO_H

#include "xgc.h"
#include "xvalue.h"
#include <stdint.h>
#include <stdbool.h>

/* 前向声明 */
typedef struct XrUpvalue XrUpvalue;

/* Upvalue 描述符 */
typedef struct UpvalDesc {
    char *name;                 /* 变量名（调试用） */
    uint8_t index;              /* 变量索引 */
    bool is_local;              /* 是否是局部变量 */
    bool is_readonly;           /* 是否只读（const） */
} UpvalDesc;

/* 函数原型 - 共享的函数定义（类似 Lua 的 Proto） */
typedef struct XrFnProto {
    GCHeader gc;                /* GC 头 */
    
    /* === 字节码和常量 === */
    uint8_t *bytecode;          /* 字节码数组 */
    XrValue *constants;         /* 常量池 */
    uint16_t bytecode_len;      /* 字节码长度 */
    uint16_t const_count;       /* 常量数量 */
    
    /* === 函数元信息 === */
    char *name;                 /* 函数名（调试用，可为 NULL） */
    uint8_t param_count;        /* 参数个数 */
    uint8_t max_stack;          /* 最大栈大小（预留给VM） */
    bool is_variadic;           /* 是否可变参数（预留） */
    
    /* === Upvalue 信息 === */
    UpvalDesc *upval_descs;     /* Upvalue 描述符数组 */
    uint16_t upval_count;       /* Upvalue 数量 */
    
    /* === 嵌套函数 === */
    struct XrFnProto **inner_protos; /* 内嵌函数原型 */
    uint16_t inner_count;       /* 内嵌函数数量 */
    
    /* === 调试信息（预留） === */
    uint32_t *line_info;        /* 行号信息 */
    char *source_file;          /* 源文件名 */
    
    /* === 类型信息（集成第七阶段）（预留） === */
    void *param_types;          /* 参数类型数组（TypeInfo**） */
    void *return_type;          /* 返回值类型（TypeInfo*） */
    
    /* === 优化标志（预留给 JIT） === */
    uint32_t call_count;        /* 调用计数（热点检测） */
    void *jit_code;             /* JIT 编译后的代码指针 */
    uint8_t optimization_level; /* 优化级别 */
    
    /* === 树遍历解释器使用（当前阶段）=== */
    void *ast_body;             /* AST节点（当前使用）*/
    
} XrFnProto;

/* 创建函数原型 */
XrFnProto* xr_proto_create(const char *name, uint8_t param_count);

/* 释放函数原型 */
void xr_proto_free(XrFnProto *proto);

/* 添加 upvalue 描述符 */
int xr_proto_add_upvalue(XrFnProto *proto, const char *name, 
                         uint8_t index, bool is_local);

/* 打印函数原型信息（调试用） */
void xr_proto_print(XrFnProto *proto);

#endif /* FN_PROTO_H */


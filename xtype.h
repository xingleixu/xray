/*
** xtype.h
** Xray 类型系统定义
** 
** v0.6.0：基础类型系统
** 支持：基本类型、数组类型、联合类型、可选类型
*/

#ifndef xtype_h
#define xtype_h

#include "xray.h"
#include <stdbool.h>

/* 前向声明 */
/* typedef struct XrayState XrayState; */ /* 已在xray.h中定义 */

/*
** 类型种类
*/
typedef enum {
    TYPE_VOID,       /* void（无返回值） */
    TYPE_NULL,       /* null类型 */
    TYPE_BOOL,       /* bool */
    TYPE_INT,        /* int */
    TYPE_FLOAT,      /* float */
    TYPE_STRING,     /* string */
    TYPE_ARRAY,      /* array<T> */
    TYPE_MAP,        /* map<K, V> */
    TYPE_FUNCTION,   /* (T1, T2) => T3 */
    TYPE_CLASS,      /* class Name */
    TYPE_ANY,        /* any（动态类型） */
    TYPE_UNION,      /* T1 | T2 */
    TYPE_OPTIONAL,   /* T?（语法糖：T | null） */
    TYPE_PARAM       /* 类型参数（如泛型中的T） */
} TypeKind;

/*
** 类型信息结构
** 描述一个类型的完整信息
*/
typedef struct XrTypeInfo {
    TypeKind kind;
    
    union {
        /* 数组类型：array<T> */
        struct {
            struct XrTypeInfo *element_type;
        } array;
        
        /* Map类型：map<K, V> */
        struct {
            struct XrTypeInfo *key_type;
            struct XrTypeInfo *value_type;
        } map;
        
        /* 函数类型：(T1, T2) => T3 */
        struct {
            struct XrTypeInfo **param_types;
            int param_count;
            struct XrTypeInfo *return_type;
        } function;
        
        /* 类类型：class Name */
        struct {
            char *class_name;
        } class_type;
        
        /* 联合类型：T1 | T2 */
        struct {
            struct XrTypeInfo **types;
            int type_count;
        } union_type;
        
        /* 可选类型：T?（等价于 T | null） */
        struct {
            struct XrTypeInfo *base_type;
        } optional;
        
        /* 类型参数：T, U等（用于泛型） */
        struct {
            char *name;       /* 参数名称："T" */
            int id;           /* 唯一标识符 */
        } type_param;
    } as;
} XrTypeInfo;

/* ========== 全局内置类型（单例）========== */

/* 基本类型 */
extern XrTypeInfo xr_builtin_void_type;
extern XrTypeInfo xr_builtin_null_type;
extern XrTypeInfo xr_builtin_bool_type;
extern XrTypeInfo xr_builtin_int_type;
extern XrTypeInfo xr_builtin_float_type;
extern XrTypeInfo xr_builtin_string_type;
extern XrTypeInfo xr_builtin_any_type;

/* ========== 类型创建函数 ========== */

/* 返回内置类型（单例） */
XrTypeInfo* xr_type_void(XrayState *X);
XrTypeInfo* xr_type_null(XrayState *X);
XrTypeInfo* xr_type_bool(XrayState *X);
XrTypeInfo* xr_type_int(XrayState *X);
XrTypeInfo* xr_type_float(XrayState *X);
XrTypeInfo* xr_type_string(XrayState *X);
XrTypeInfo* xr_type_any(XrayState *X);

/* 创建复合类型（需要分配内存） */
XrTypeInfo* xr_type_array(XrayState *X, XrTypeInfo *element_type);
XrTypeInfo* xr_type_map(XrayState *X, XrTypeInfo *key_type, XrTypeInfo *value_type);
XrTypeInfo* xr_type_function(XrayState *X, XrTypeInfo **param_types, 
                              int param_count, XrTypeInfo *return_type);
XrTypeInfo* xr_type_union(XrayState *X, XrTypeInfo **types, int type_count);
XrTypeInfo* xr_type_optional(XrayState *X, XrTypeInfo *base_type);

/* ========== 类型操作 ========== */

/* 类型比较 */
bool xr_type_equals(XrTypeInfo *a, XrTypeInfo *b);
bool xr_type_is_assignable(XrTypeInfo *from, XrTypeInfo *to);

/* 类型检查（用于运行时） */
/* 注意：XrValue定义在xvalue.h中，使用时需要包含xvalue.h */
struct XrValue;  /* 前向声明 */
bool xr_type_check_value(struct XrValue *value, XrTypeInfo *expected);

/* 类型转字符串（用于错误提示） */
char* xr_type_to_string(XrTypeInfo *type);
const char* xr_type_kind_name(TypeKind kind);

/* 类型内存管理 */
void xr_type_free(XrTypeInfo *type);

/* ========== 类型推导 ========== */

/* 前向声明AST节点 */
struct AstNode;

/* 从表达式推导类型 */
XrTypeInfo* xr_infer_type_from_expr(XrayState *X, struct AstNode *expr);

/* 从字面量推导类型 */
XrTypeInfo* xr_infer_literal_type(XrayState *X, struct AstNode *literal);

/* 从二元表达式推导类型 */
XrTypeInfo* xr_infer_binary_type(XrayState *X, struct AstNode *binary);

/* 从一元表达式推导类型 */
XrTypeInfo* xr_infer_unary_type(XrayState *X, struct AstNode *unary);

/* 类型提升（int + float → float） */
XrTypeInfo* xr_type_promote(XrayState *X, XrTypeInfo *t1, XrTypeInfo *t2);

/* 从函数体推导返回类型 */
XrTypeInfo* xr_infer_function_return_type(XrayState *X, struct AstNode *func_body);

/* 从语句中收集返回类型 */
XrTypeInfo* xr_collect_return_types(XrayState *X, struct AstNode *stmt, XrTypeInfo *current_type);

/* ========== 类型别名（Type Alias） ========== */

/* 类型别名条目 */
typedef struct {
    char *name;             /* 别名名称 */
    XrTypeInfo *type;       /* 目标类型 */
} TypeAlias;

/* 类型别名表 */
typedef struct {
    TypeAlias *entries;     /* 别名数组 */
    int count;              /* 当前数量 */
    int capacity;           /* 容量 */
} TypeAliasTable;

/* 注册类型别名 */
void xr_register_type_alias(XrayState *X, const char *name, XrTypeInfo *type);

/* 查找类型别名 */
XrTypeInfo* xr_resolve_type_alias(XrayState *X, const char *name);

/* 递归解析类型别名（处理别名的别名） */
XrTypeInfo* xr_resolve_type_alias_recursive(XrayState *X, const char *name);

/* 初始化类型别名表 */
void xr_type_alias_init(XrayState *X);

/* 释放类型别名表 */
void xr_type_alias_free(XrayState *X);

/* ========== 泛型类型参数 ========== */

/* 创建类型参数（用于泛型）*/
XrTypeInfo* xr_type_param(XrayState *X, const char *name, int id);

/* 类型参数映射（用于泛型实例化） */
typedef struct {
    char *param_name;       /* 参数名："T" */
    XrTypeInfo *actual_type;  /* 实际类型：int */
} TypeParamBinding;

typedef struct {
    TypeParamBinding *bindings;
    int count;
    int capacity;
} TypeParamMap;

/* 创建类型参数映射 */
TypeParamMap* xr_type_param_map_new(void);

/* 释放类型参数映射 */
void xr_type_param_map_free(TypeParamMap *map);

/* 添加类型参数绑定 */
void xr_type_param_map_add(TypeParamMap *map, const char *param_name, XrTypeInfo *actual_type);

/* 查找类型参数绑定 */
XrTypeInfo* xr_type_param_map_lookup(TypeParamMap *map, const char *param_name);

/* 类型替换：将类型中的类型参数替换为实际类型 */
XrTypeInfo* xr_type_substitute(XrayState *X, XrTypeInfo *type, TypeParamMap *map);

/* ========== 类型系统初始化 ========== */

/* 初始化内置类型（在XrayState创建时调用） */
void xr_type_init(XrayState *X);

#endif /* xtype_h */


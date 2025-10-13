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
    TYPE_OPTIONAL    /* T?（语法糖：T | null） */
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

/* ========== 类型系统初始化 ========== */

/* 初始化内置类型（在XrayState创建时调用） */
void xr_type_init(XrayState *X);

#endif /* xtype_h */


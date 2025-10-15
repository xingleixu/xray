/*
** xvalue.c
** Xray 值对象系统实现
*/

#include "xvalue.h"
#include "xtype.h"
#include "xstring.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* ========== 值创建函数（新API）========== */

/*
** 创建null值
*/
XrValue xr_null(void) {
#if XR_NAN_TAGGING
    return XR_NULL_VAL;
#else
    XrValue v;
    v.type = XR_TNULL;
    v.type_info = &xr_builtin_null_type;
    v.as.i = 0;  /* 初始化union */
    return v;
#endif
}

/*
** 创建布尔值
*/
XrValue xr_bool(int b) {
#if XR_NAN_TAGGING
    return b ? XR_TRUE_VAL : XR_FALSE_VAL;
#else
    XrValue v;
    v.type = XR_TBOOL;
    v.type_info = &xr_builtin_bool_type;
    v.as.b = b ? 1 : 0;
    return v;
#endif
}

/*
** 创建整数值（使用内置类型）
*/
XrValue xr_int(xr_Integer i) {
    return xr_make_int(i, &xr_builtin_int_type);
}

/*
** 创建浮点数值（使用内置类型）
*/
XrValue xr_float(xr_Number n) {
    return xr_make_float(n, &xr_builtin_float_type);
}

/*
** 创建带类型信息的整数值
*/
XrValue xr_make_int(xr_Integer i, XrTypeInfo *type_info) {
#if XR_NAN_TAGGING
    /* NaN Tagging模式：整数作为浮点数存储（第13阶段优化） */
    return wrenDoubleToBits((double)i);
#else
    XrValue v;
    v.type = XR_TINT;
    v.type_info = type_info ? type_info : &xr_builtin_int_type;
    v.as.i = i;
    return v;
#endif
}

/*
** 创建带类型信息的浮点数值
*/
XrValue xr_make_float(xr_Number n, XrTypeInfo *type_info) {
#if XR_NAN_TAGGING
    return wrenDoubleToBits(n);
#else
    XrValue v;
    v.type = XR_TFLOAT;
    v.type_info = type_info ? type_info : &xr_builtin_float_type;
    v.as.n = n;
    return v;
#endif
}

/*
** 创建带类型信息的布尔值
*/
XrValue xr_make_bool(int b, XrTypeInfo *type_info) {
#if XR_NAN_TAGGING
    return b ? XR_TRUE_VAL : XR_FALSE_VAL;
#else
    XrValue v;
    v.type = XR_TBOOL;
    v.type_info = type_info ? type_info : &xr_builtin_bool_type;
    v.as.b = b ? 1 : 0;
    return v;
#endif
}

/* ========== 类型信息访问 ========== */

/*
** 获取值的类型信息
*/
XrTypeInfo* xr_typeof(const XrValue *v) {
#if XR_NAN_TAGGING
    /* NaN Tagging模式 */
    if (XR_IS_OBJ(*v)) {
        /* 对象：从对象头部获取 */
        XrObject *obj = (XrObject*)XR_TO_OBJ(*v);
        return obj->type_info;
    } else if (XR_IS_NUM(*v)) {
        /* 数字：返回float类型（暂时简化，第13阶段区分int/float） */
        return &xr_builtin_float_type;
    } else if (XR_IS_NULL(*v)) {
        return &xr_builtin_null_type;
    } else if (XR_IS_BOOL(*v)) {
        return &xr_builtin_bool_type;
    }
    return &xr_builtin_any_type;
#else
    /* Tagged Union模式：直接返回 */
    assert(v->type_info != NULL);
    return v->type_info;
#endif
}

/*
** 获取类型名称字符串
*/
const char* xr_typename_str(const XrValue *v) {
    XrTypeInfo *type = xr_typeof(v);
    return xr_type_kind_name(type->kind);
}

/*
** 检查值是否匹配类型
*/
bool xr_value_is_type(const XrValue *v, XrTypeInfo *expected) {
    if (expected == NULL || expected->kind == TYPE_ANY) {
        return true;  /* any类型接受所有值 */
    }
    
    XrTypeInfo *actual = xr_typeof(v);
    return xr_type_equals(actual, expected);
}

/* ========== 基本类型名称 ========== */

/*
** 获取基本类型的名称
*/
const char* xr_typename(XrType type) {
    switch (type) {
        case XR_TNULL:     return "null";
        case XR_TBOOL:     return "bool";
        case XR_TINT:      return "int";
        case XR_TFLOAT:    return "float";
        case XR_TSTRING:   return "string";
        case XR_TFUNCTION: return "function";
        case XR_TARRAY:    return "array";
        case XR_TSET:      return "set";
        case XR_TMAP:      return "map";
        case XR_TCLASS:    return "class";
        default:           return "unknown";
    }
}

/* ========== 对象操作 ========== */

/*
** 初始化对象头部
*/
void xr_object_init(XrObject *obj, XrType type, XrTypeInfo *type_info) {
    obj->next = NULL;
    obj->type = type;
    obj->type_info = type_info;
    obj->marked = false;
}

/*
** 创建字符串值（新增 v0.10.0）
** 使用新的字符串系统（xstring.h/c）
*/
XrValue xr_string_value(XrString *str) {
#if XR_NAN_TAGGING
    /* NaN Tagging模式：对象指针 */
    return XR_OBJ_TO_VAL(str);
#else
    /* Tagged Union模式 */
    XrValue v;
    v.type = XR_TSTRING;
    v.type_info = str ? str->header.type_info : NULL;
    v.as.obj = str;
    return v;
#endif
}

/*
** 创建函数对象
*/
XrFunction* xr_function_new(const char *name, char **parameters, 
                            XrTypeInfo **param_types, int param_count,
                            XrTypeInfo *return_type, AstNode *body) {
    XrFunction *func = (XrFunction*)malloc(sizeof(XrFunction));
    if (func == NULL) {
        return NULL;
    }
    
    xr_object_init(&func->header, XR_TFUNCTION, NULL);  /* TODO: 函数类型 */
    
    /* 复制函数名 */
    func->name = name ? strdup(name) : NULL;
    
    /* 复制参数列表 */
    func->param_count = param_count;
    if (param_count > 0) {
        func->parameters = (char**)malloc(sizeof(char*) * param_count);
        func->param_types = (XrTypeInfo**)malloc(sizeof(XrTypeInfo*) * param_count);
        for (int i = 0; i < param_count; i++) {
            func->parameters[i] = parameters[i] ? strdup(parameters[i]) : NULL;
            func->param_types[i] = param_types ? param_types[i] : &xr_builtin_any_type;
        }
    } else {
        func->parameters = NULL;
        func->param_types = NULL;
    }
    
    /* 设置返回类型 */
    func->return_type = return_type ? return_type : &xr_builtin_any_type;
    
    /* 设置函数体和闭包 */
    func->body = body;
    func->closure_scope = NULL;
    
    /* 初始化闭包支持字段（第八阶段新增） */
    func->captured_vars = NULL;
    func->captured_values = NULL;
    func->captured_count = 0;
    
    return func;
}

/*
** 释放函数对象
*/
void xr_function_free(XrFunction *func) {
    if (func == NULL) {
        return;
    }
    
    /* 释放函数名 */
    if (func->name) {
        free(func->name);
    }
    
    /* 释放参数列表 */
    if (func->parameters) {
        for (int i = 0; i < func->param_count; i++) {
            if (func->parameters[i]) {
                free(func->parameters[i]);
            }
        }
        free(func->parameters);
    }
    
    /* 释放参数类型列表 */
    if (func->param_types) {
        free(func->param_types);
    }
    
    /* 释放捕获的变量（第八阶段新增） */
    if (func->captured_vars) {
        for (int i = 0; i < func->captured_count; i++) {
            if (func->captured_vars[i]) {
                free(func->captured_vars[i]);
            }
        }
        free(func->captured_vars);
    }
    if (func->captured_values) {
        free(func->captured_values);
    }
    
    /* 释放函数对象本身 */
    free(func);
}

/* ========== 数组值操作========== */

/*
** 创建数组值
*/
XrValue xr_value_from_array(struct XrArray *arr) {
#if XR_NAN_TAGGING
    /* NaN Tagging模式：对象指针 */
    return XR_OBJ_TO_VAL(arr);
#else
    /* Tagged Union模式 */
    XrValue v;
    v.type = XR_TARRAY;
    v.type_info = NULL;  /* TODO: 数组类型信息 */
    v.as.obj = arr;
    return v;
#endif
}

/*
** 检查值是否为数组
*/
bool xr_value_is_array(XrValue v) {
#if XR_NAN_TAGGING
    if (!XR_IS_OBJ(v)) return false;
    XrObject *obj = (XrObject*)XR_TO_OBJ(v);
    return obj->type == XR_TARRAY;
#else
    return v.type == XR_TARRAY;
#endif
}

/*
** 从值中提取数组对象
*/
struct XrArray* xr_value_to_array(XrValue v) {
#if XR_NAN_TAGGING
    if (!XR_IS_OBJ(v)) return NULL;
    return (struct XrArray*)XR_TO_OBJ(v);
#else
    if (v.type != XR_TARRAY) return NULL;
    return (struct XrArray*)v.as.obj;
#endif
}

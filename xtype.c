/*
** xtype.c
** Xray 类型系统实现
*/

#include "xtype.h"
#include "xvalue.h"
#include "xstate.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* ========== 全局内置类型（单例）========== */

XrTypeInfo xr_builtin_void_type = { TYPE_VOID };
XrTypeInfo xr_builtin_null_type = { TYPE_NULL };
XrTypeInfo xr_builtin_bool_type = { TYPE_BOOL };
XrTypeInfo xr_builtin_int_type = { TYPE_INT };
XrTypeInfo xr_builtin_float_type = { TYPE_FLOAT };
XrTypeInfo xr_builtin_string_type = { TYPE_STRING };
XrTypeInfo xr_builtin_any_type = { TYPE_ANY };

/* ========== 类型创建函数 ========== */

/*
** 返回内置类型（单例）
*/
XrTypeInfo* xr_type_void(XrayState *X) {
    (void)X;  /* 暂未使用 */
    return &xr_builtin_void_type;
}

XrTypeInfo* xr_type_null(XrayState *X) {
    (void)X;
    return &xr_builtin_null_type;
}

XrTypeInfo* xr_type_bool(XrayState *X) {
    (void)X;
    return &xr_builtin_bool_type;
}

XrTypeInfo* xr_type_int(XrayState *X) {
    (void)X;
    return &xr_builtin_int_type;
}

XrTypeInfo* xr_type_float(XrayState *X) {
    (void)X;
    return &xr_builtin_float_type;
}

XrTypeInfo* xr_type_string(XrayState *X) {
    (void)X;
    return &xr_builtin_string_type;
}

XrTypeInfo* xr_type_any(XrayState *X) {
    (void)X;
    return &xr_builtin_any_type;
}

/*
** 创建数组类型
*/
XrTypeInfo* xr_type_array(XrayState *X, XrTypeInfo *element_type) {
    (void)X;  /* TODO: 使用XrayState的内存分配器 */
    
    XrTypeInfo *type = (XrTypeInfo*)malloc(sizeof(XrTypeInfo));
    if (type == NULL) {
        return NULL;
    }
    
    type->kind = TYPE_ARRAY;
    type->as.array.element_type = element_type;
    
    return type;
}

/*
** 创建Map类型
*/
XrTypeInfo* xr_type_map(XrayState *X, XrTypeInfo *key_type, XrTypeInfo *value_type) {
    (void)X;
    
    XrTypeInfo *type = (XrTypeInfo*)malloc(sizeof(XrTypeInfo));
    if (type == NULL) {
        return NULL;
    }
    
    type->kind = TYPE_MAP;
    type->as.map.key_type = key_type;
    type->as.map.value_type = value_type;
    
    return type;
}

/*
** 创建函数类型
*/
XrTypeInfo* xr_type_function(XrayState *X, XrTypeInfo **param_types, 
                              int param_count, XrTypeInfo *return_type) {
    (void)X;
    
    XrTypeInfo *type = (XrTypeInfo*)malloc(sizeof(XrTypeInfo));
    if (type == NULL) {
        return NULL;
    }
    
    type->kind = TYPE_FUNCTION;
    type->as.function.param_count = param_count;
    type->as.function.return_type = return_type;
    
    if (param_count > 0) {
        type->as.function.param_types = (XrTypeInfo**)malloc(sizeof(XrTypeInfo*) * param_count);
        for (int i = 0; i < param_count; i++) {
            type->as.function.param_types[i] = param_types[i];
        }
    } else {
        type->as.function.param_types = NULL;
    }
    
    return type;
}

/*
** 创建联合类型
*/
XrTypeInfo* xr_type_union(XrayState *X, XrTypeInfo **types, int type_count) {
    (void)X;
    
    if (type_count < 2) {
        /* 联合类型至少需要2个类型 */
        return type_count == 1 ? types[0] : &xr_builtin_any_type;
    }
    
    XrTypeInfo *type = (XrTypeInfo*)malloc(sizeof(XrTypeInfo));
    if (type == NULL) {
        return NULL;
    }
    
    type->kind = TYPE_UNION;
    type->as.union_type.type_count = type_count;
    type->as.union_type.types = (XrTypeInfo**)malloc(sizeof(XrTypeInfo*) * type_count);
    
    for (int i = 0; i < type_count; i++) {
        type->as.union_type.types[i] = types[i];
    }
    
    return type;
}

/*
** 创建可选类型（语法糖：T? = T | null）
*/
XrTypeInfo* xr_type_optional(XrayState *X, XrTypeInfo *base_type) {
    XrTypeInfo *types[2] = { base_type, &xr_builtin_null_type };
    return xr_type_union(X, types, 2);
}

/* ========== 类型操作 ========== */

/*
** 比较两个类型是否相等
*/
bool xr_type_equals(XrTypeInfo *a, XrTypeInfo *b) {
    if (a == b) {
        return true;  /* 同一个对象或都为NULL */
    }
    
    if (a == NULL || b == NULL) {
        return false;
    }
    
    if (a->kind != b->kind) {
        return false;
    }
    
    switch (a->kind) {
        case TYPE_VOID:
        case TYPE_NULL:
        case TYPE_BOOL:
        case TYPE_INT:
        case TYPE_FLOAT:
        case TYPE_STRING:
        case TYPE_ANY:
            /* 基本类型：kind相同即相等 */
            return true;
            
        case TYPE_ARRAY:
            /* 数组类型：元素类型相同 */
            return xr_type_equals(a->as.array.element_type, b->as.array.element_type);
            
        case TYPE_MAP:
            /* Map类型：键和值类型都相同 */
            return xr_type_equals(a->as.map.key_type, b->as.map.key_type) &&
                   xr_type_equals(a->as.map.value_type, b->as.map.value_type);
                   
        case TYPE_FUNCTION:
            /* 函数类型：参数和返回值类型都相同 */
            if (a->as.function.param_count != b->as.function.param_count) {
                return false;
            }
            for (int i = 0; i < a->as.function.param_count; i++) {
                if (!xr_type_equals(a->as.function.param_types[i], 
                                   b->as.function.param_types[i])) {
                    return false;
                }
            }
            return xr_type_equals(a->as.function.return_type, b->as.function.return_type);
            
        case TYPE_UNION:
            /* 联合类型：成员类型相同（顺序无关，暂时简化为顺序相关） */
            if (a->as.union_type.type_count != b->as.union_type.type_count) {
                return false;
            }
            for (int i = 0; i < a->as.union_type.type_count; i++) {
                if (!xr_type_equals(a->as.union_type.types[i], 
                                   b->as.union_type.types[i])) {
                    return false;
                }
            }
            return true;
            
        default:
            return false;
    }
}

/*
** 检查from类型是否可赋值给to类型
*/
bool xr_type_is_assignable(XrTypeInfo *from, XrTypeInfo *to) {
    /* any类型接受所有值 */
    if (to->kind == TYPE_ANY) {
        return true;
    }
    
    /* 相等类型可赋值 */
    if (xr_type_equals(from, to)) {
        return true;
    }
    
    /* 联合类型：from是to的成员之一 */
    if (to->kind == TYPE_UNION) {
        for (int i = 0; i < to->as.union_type.type_count; i++) {
            if (xr_type_is_assignable(from, to->as.union_type.types[i])) {
                return true;
            }
        }
    }
    
    /* TODO: 子类型关系（第12阶段OOP） */
    
    return false;
}

/*
** 运行时类型检查
*/
bool xr_type_check_value(struct XrValue *value, XrTypeInfo *expected) {
    XrValue *v = (XrValue*)value;  /* 转换为具体类型 */
    if (expected == NULL || expected->kind == TYPE_ANY) {
        return true;  /* any类型接受所有值 */
    }
    
    /* 获取值的实际类型 */
    XrTypeInfo *actual = xr_typeof(v);
    
    /* 使用类型可赋值性检查 */
    return xr_type_is_assignable(actual, expected);
}

/* ========== 类型转字符串 ========== */

/*
** 获取类型种类的名称
*/
const char* xr_type_kind_name(TypeKind kind) {
    switch (kind) {
        case TYPE_VOID:     return "void";
        case TYPE_NULL:     return "null";
        case TYPE_BOOL:     return "bool";
        case TYPE_INT:      return "int";
        case TYPE_FLOAT:    return "float";
        case TYPE_STRING:   return "string";
        case TYPE_ARRAY:    return "array";
        case TYPE_MAP:      return "map";
        case TYPE_FUNCTION: return "function";
        case TYPE_CLASS:    return "class";
        case TYPE_ANY:      return "any";
        case TYPE_UNION:    return "union";
        case TYPE_OPTIONAL: return "optional";
        default:            return "unknown";
    }
}

/*
** 类型转字符串（递归）
*/
char* xr_type_to_string(XrTypeInfo *type) {
    if (type == NULL) {
        return strdup("unknown");
    }
    
    static char buffer[256];
    
    switch (type->kind) {
        case TYPE_VOID:
        case TYPE_NULL:
        case TYPE_BOOL:
        case TYPE_INT:
        case TYPE_FLOAT:
        case TYPE_STRING:
        case TYPE_ANY:
            return strdup(xr_type_kind_name(type->kind));
            
        case TYPE_ARRAY: {
            char *elem = xr_type_to_string(type->as.array.element_type);
            snprintf(buffer, sizeof(buffer), "%s[]", elem);
            free(elem);
            return strdup(buffer);
        }
        
        case TYPE_MAP: {
            char *key = xr_type_to_string(type->as.map.key_type);
            char *val = xr_type_to_string(type->as.map.value_type);
            snprintf(buffer, sizeof(buffer), "map<%s, %s>", key, val);
            free(key);
            free(val);
            return strdup(buffer);
        }
        
        case TYPE_UNION: {
            /* 简化处理：只显示第一个和最后一个类型 */
            if (type->as.union_type.type_count >= 2) {
                char *first = xr_type_to_string(type->as.union_type.types[0]);
                char *last = xr_type_to_string(type->as.union_type.types[
                    type->as.union_type.type_count - 1]);
                snprintf(buffer, sizeof(buffer), "%s | %s", first, last);
                free(first);
                free(last);
                return strdup(buffer);
            }
            return strdup("union");
        }
        
        default:
            return strdup(xr_type_kind_name(type->kind));
    }
}

/* ========== 类型内存管理 ========== */

/*
** 释放类型对象
*/
void xr_type_free(XrTypeInfo *type) {
    if (type == NULL) {
        return;
    }
    
    /* 不释放内置类型 */
    if (type == &xr_builtin_void_type ||
        type == &xr_builtin_null_type ||
        type == &xr_builtin_bool_type ||
        type == &xr_builtin_int_type ||
        type == &xr_builtin_float_type ||
        type == &xr_builtin_string_type ||
        type == &xr_builtin_any_type) {
        return;
    }
    
    /* 释放复合类型的内部数据 */
    switch (type->kind) {
        case TYPE_FUNCTION:
            if (type->as.function.param_types) {
                free(type->as.function.param_types);
            }
            break;
            
        case TYPE_UNION:
            if (type->as.union_type.types) {
                free(type->as.union_type.types);
            }
            break;
            
        default:
            break;
    }
    
    /* 释放类型对象本身 */
    free(type);
}

/* ========== 类型系统初始化 ========== */

/*
** 初始化内置类型
*/
void xr_type_init(XrayState *X) {
    (void)X;  /* 暂未使用 */
    /* 内置类型是全局静态变量，已自动初始化 */
}


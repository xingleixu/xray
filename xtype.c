/*
** xtype.c
** Xray 类型系统实现
*/

#include "xtype.h"
#include "xvalue.h"
#include "xstate.h"
#include "xast.h"  /* 需要AST节点定义进行类型推导 */
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

/*
** 创建类型参数（用于泛型）
** 例如：function identity<T>(x: T): T 中的 T
*/
XrTypeInfo* xr_type_param(XrayState *X, const char *name, int id) {
    (void)X;  /* TODO: 使用XrayState的内存分配器 */
    
    XrTypeInfo *type = (XrTypeInfo*)malloc(sizeof(XrTypeInfo));
    if (type == NULL) {
        return NULL;
    }
    
    type->kind = TYPE_PARAM;
    type->as.type_param.name = strdup(name);
    type->as.type_param.id = id;
    
    return type;
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
        case TYPE_PARAM:    return "type_param";
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

/* ========== 类型推导 ========== */

/*
** 从字面量推导类型
*/
XrTypeInfo* xr_infer_literal_type(XrayState *X, struct AstNode *literal) {
    if (literal == NULL) {
        return NULL;
    }
    
    switch (literal->type) {
        case AST_LITERAL_INT:
            return xr_type_int(X);
            
        case AST_LITERAL_FLOAT:
            return xr_type_float(X);
            
        case AST_LITERAL_STRING:
            return xr_type_string(X);
            
        case AST_LITERAL_TRUE:
        case AST_LITERAL_FALSE:
            return xr_type_bool(X);
            
        case AST_LITERAL_NULL:
            return xr_type_null(X);
            
        default:
            return NULL;
    }
}

/*
** 类型提升规则
** int + int   → int
** int + float → float
** float + float → float
*/
XrTypeInfo* xr_type_promote(XrayState *X, XrTypeInfo *t1, XrTypeInfo *t2) {
    if (t1 == NULL || t2 == NULL) {
        return NULL;
    }
    
    /* 如果类型相同，返回相同类型 */
    if (xr_type_equals(t1, t2)) {
        return t1;
    }
    
    /* 数值类型提升：int + float → float */
    if ((t1->kind == TYPE_INT && t2->kind == TYPE_FLOAT) ||
        (t1->kind == TYPE_FLOAT && t2->kind == TYPE_INT)) {
        return xr_type_float(X);
    }
    
    /* 其他情况：类型不兼容 */
    return NULL;
}

/*
** 从二元表达式推导类型
*/
XrTypeInfo* xr_infer_binary_type(XrayState *X, struct AstNode *binary) {
    if (binary == NULL) {
        return NULL;
    }
    
    /* 递归推导左右操作数的类型 */
    XrTypeInfo *left_type = xr_infer_type_from_expr(X, binary->as.binary.left);
    XrTypeInfo *right_type = xr_infer_type_from_expr(X, binary->as.binary.right);
    
    if (left_type == NULL || right_type == NULL) {
        return NULL;
    }
    
    switch (binary->type) {
        /* 算术运算：返回数值类型（应用类型提升规则） */
        case AST_BINARY_ADD:
        case AST_BINARY_SUB:
        case AST_BINARY_MUL:
        case AST_BINARY_DIV:
        case AST_BINARY_MOD:
            return xr_type_promote(X, left_type, right_type);
            
        /* 比较运算：返回bool */
        case AST_BINARY_EQ:
        case AST_BINARY_NE:
        case AST_BINARY_LT:
        case AST_BINARY_LE:
        case AST_BINARY_GT:
        case AST_BINARY_GE:
            return xr_type_bool(X);
            
        /* 逻辑运算：返回bool */
        case AST_BINARY_AND:
        case AST_BINARY_OR:
            return xr_type_bool(X);
            
        default:
            return NULL;
    }
}

/*
** 从一元表达式推导类型
*/
XrTypeInfo* xr_infer_unary_type(XrayState *X, struct AstNode *unary) {
    if (unary == NULL) {
        return NULL;
    }
    
    /* 递归推导操作数的类型 */
    XrTypeInfo *operand_type = xr_infer_type_from_expr(X, unary->as.unary.operand);
    
    if (operand_type == NULL) {
        return NULL;
    }
    
    switch (unary->type) {
        case AST_UNARY_NEG:
            /* 取负：返回相同数值类型 */
            if (operand_type->kind == TYPE_INT || operand_type->kind == TYPE_FLOAT) {
                return operand_type;
            }
            return NULL;
            
        case AST_UNARY_NOT:
            /* 逻辑非：返回bool */
            return xr_type_bool(X);
            
        default:
            return NULL;
    }
}

/*
** 从表达式推导类型（主入口函数）
*/
XrTypeInfo* xr_infer_type_from_expr(XrayState *X, struct AstNode *expr) {
    if (expr == NULL) {
        return NULL;
    }
    
    switch (expr->type) {
        /* 字面量 */
        case AST_LITERAL_INT:
        case AST_LITERAL_FLOAT:
        case AST_LITERAL_STRING:
        case AST_LITERAL_TRUE:
        case AST_LITERAL_FALSE:
        case AST_LITERAL_NULL:
            return xr_infer_literal_type(X, expr);
            
        /* 二元运算 */
        case AST_BINARY_ADD:
        case AST_BINARY_SUB:
        case AST_BINARY_MUL:
        case AST_BINARY_DIV:
        case AST_BINARY_MOD:
        case AST_BINARY_EQ:
        case AST_BINARY_NE:
        case AST_BINARY_LT:
        case AST_BINARY_LE:
        case AST_BINARY_GT:
        case AST_BINARY_GE:
        case AST_BINARY_AND:
        case AST_BINARY_OR:
            return xr_infer_binary_type(X, expr);
            
        /* 一元运算 */
        case AST_UNARY_NEG:
        case AST_UNARY_NOT:
            return xr_infer_unary_type(X, expr);
            
        /* 分组表达式：返回内部表达式的类型 */
        case AST_GROUPING:
            return xr_infer_type_from_expr(X, expr->as.grouping);
            
        /* 变量引用：从符号表查找类型（TODO：第七阶段后期实现） */
        case AST_VARIABLE:
            /* TODO: 从符号表查找变量的类型 */
            return xr_type_any(X);  /* 暂时返回any */
            
        /* 函数调用：从函数定义获取返回类型（TODO） */
        case AST_CALL_EXPR:
            /* TODO: 从函数定义获取返回类型 */
            return xr_type_any(X);  /* 暂时返回any */
            
        default:
            /* 其他节点类型：暂不支持 */
            return NULL;
    }
}

/*
** 从语句中递归收集返回类型
** 遍历语句树，找到所有return语句并统一它们的类型
*/
XrTypeInfo* xr_collect_return_types(XrayState *X, struct AstNode *stmt, XrTypeInfo *current_type) {
    if (stmt == NULL) {
        return current_type;
    }
    
    switch (stmt->type) {
        case AST_RETURN_STMT: {
            /* 找到return语句，推导其返回值类型 */
            if (stmt->as.return_stmt.value == NULL) {
                /* return 无返回值 → void */
                XrTypeInfo *void_type = xr_type_void(X);
                if (current_type == NULL) {
                    return void_type;
                }
                /* 检查类型一致性 */
                if (!xr_type_equals(current_type, void_type)) {
                    return NULL;  /* 类型不一致 */
                }
                return current_type;
            } else {
                /* return expr → 推导表达式类型 */
                XrTypeInfo *return_type = xr_infer_type_from_expr(X, stmt->as.return_stmt.value);
                if (return_type == NULL) {
                    return NULL;
                }
                
                if (current_type == NULL) {
                    return return_type;
                }
                
                /* 检查类型一致性 */
                if (!xr_type_equals(current_type, return_type)) {
                    return NULL;  /* 类型不一致 */
                }
                
                return current_type;
            }
        }
        
        case AST_BLOCK: {
            /* 代码块：遍历所有语句 */
            for (int i = 0; i < stmt->as.block.count; i++) {
                current_type = xr_collect_return_types(X, stmt->as.block.statements[i], current_type);
                if (current_type == NULL) {
                    return NULL;  /* 类型冲突 */
                }
            }
            return current_type;
        }
        
        case AST_IF_STMT: {
            /* if语句：检查then分支和else分支 */
            current_type = xr_collect_return_types(X, stmt->as.if_stmt.then_branch, current_type);
            if (stmt->as.if_stmt.else_branch != NULL) {
                current_type = xr_collect_return_types(X, stmt->as.if_stmt.else_branch, current_type);
            }
            return current_type;
        }
        
        case AST_WHILE_STMT: {
            /* while循环：检查循环体 */
            return xr_collect_return_types(X, stmt->as.while_stmt.body, current_type);
        }
        
        case AST_FOR_STMT: {
            /* for循环：检查循环体 */
            return xr_collect_return_types(X, stmt->as.for_stmt.body, current_type);
        }
        
        default:
            /* 其他语句类型：不包含return */
            return current_type;
    }
}

/*
** 从函数体推导返回类型
** 遍历函数体中的所有return语句，确保类型一致
*/
XrTypeInfo* xr_infer_function_return_type(XrayState *X, struct AstNode *func_body) {
    if (func_body == NULL) {
        return xr_type_void(X);
    }
    
    /* 收集所有return语句的类型 */
    XrTypeInfo *return_type = xr_collect_return_types(X, func_body, NULL);
    
    if (return_type == NULL) {
        /* 没有return语句或类型冲突 */
        return xr_type_void(X);
    }
    
    return return_type;
}

/* ========== 类型别名实现 ========== */

/*
** 初始化类型别名表
*/
void xr_type_alias_init(XrayState *X) {
    TypeAliasTable *table = (TypeAliasTable*)malloc(sizeof(TypeAliasTable));
    if (table == NULL) {
        return;
    }
    
    table->count = 0;
    table->capacity = 16;  /* 初始容量 */
    table->entries = (TypeAlias*)malloc(sizeof(TypeAlias) * table->capacity);
    
    X->type_aliases = table;
}

/*
** 释放类型别名表
*/
void xr_type_alias_free(XrayState *X) {
    if (X->type_aliases == NULL) {
        return;
    }
    
    TypeAliasTable *table = (TypeAliasTable*)X->type_aliases;
    
    /* 释放所有别名名称 */
    for (int i = 0; i < table->count; i++) {
        free(table->entries[i].name);
    }
    
    /* 释放数组 */
    free(table->entries);
    free(table);
    
    X->type_aliases = NULL;
}

/*
** 注册类型别名
** 例如：type UserId = int
*/
void xr_register_type_alias(XrayState *X, const char *name, XrTypeInfo *type) {
    if (X->type_aliases == NULL) {
        xr_type_alias_init(X);
    }
    
    TypeAliasTable *table = (TypeAliasTable*)X->type_aliases;
    
    /* 检查是否已存在 */
    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->entries[i].name, name) == 0) {
            /* 更新现有别名 */
            table->entries[i].type = type;
            return;
        }
    }
    
    /* 扩容检查 */
    if (table->count >= table->capacity) {
        table->capacity *= 2;
        table->entries = (TypeAlias*)realloc(table->entries, 
                                             sizeof(TypeAlias) * table->capacity);
    }
    
    /* 添加新别名 */
    table->entries[table->count].name = strdup(name);
    table->entries[table->count].type = type;
    table->count++;
}

/*
** 查找类型别名
*/
XrTypeInfo* xr_resolve_type_alias(XrayState *X, const char *name) {
    if (X->type_aliases == NULL || name == NULL) {
        return NULL;
    }
    
    TypeAliasTable *table = (TypeAliasTable*)X->type_aliases;
    
    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->entries[i].name, name) == 0) {
            return table->entries[i].type;
        }
    }
    
    return NULL;
}

/*
** 递归解析类型别名
** 处理别名的别名：type A = B; type B = C; type C = int
*/
XrTypeInfo* xr_resolve_type_alias_recursive(XrayState *X, const char *name) {
    XrTypeInfo *type = xr_resolve_type_alias(X, name);
    
    if (type == NULL) {
        return NULL;
    }
    
    /* 如果类型本身还是别名，继续递归解析 */
    /* 注意：这里简化处理，假设类型别名不会循环引用 */
    /* TODO: 添加循环检测 */
    
    return type;
}

/* ========== 泛型类型参数实现 ========== */

/*
** 创建类型参数映射
*/
TypeParamMap* xr_type_param_map_new(void) {
    TypeParamMap *map = (TypeParamMap*)malloc(sizeof(TypeParamMap));
    if (map == NULL) {
        return NULL;
    }
    
    map->count = 0;
    map->capacity = 4;  /* 初始容量，一般泛型参数不会太多 */
    map->bindings = (TypeParamBinding*)malloc(sizeof(TypeParamBinding) * map->capacity);
    
    return map;
}

/*
** 释放类型参数映射
*/
void xr_type_param_map_free(TypeParamMap *map) {
    if (map == NULL) {
        return;
    }
    
    for (int i = 0; i < map->count; i++) {
        free(map->bindings[i].param_name);
    }
    
    free(map->bindings);
    free(map);
}

/*
** 添加类型参数绑定
** 例如：T → int, U → string
*/
void xr_type_param_map_add(TypeParamMap *map, const char *param_name, XrTypeInfo *actual_type) {
    if (map == NULL) {
        return;
    }
    
    /* 检查是否已存在 */
    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->bindings[i].param_name, param_name) == 0) {
            /* 更新现有绑定 */
            map->bindings[i].actual_type = actual_type;
            return;
        }
    }
    
    /* 扩容检查 */
    if (map->count >= map->capacity) {
        map->capacity *= 2;
        map->bindings = (TypeParamBinding*)realloc(map->bindings, 
                                                   sizeof(TypeParamBinding) * map->capacity);
    }
    
    /* 添加新绑定 */
    map->bindings[map->count].param_name = strdup(param_name);
    map->bindings[map->count].actual_type = actual_type;
    map->count++;
}

/*
** 查找类型参数绑定
*/
XrTypeInfo* xr_type_param_map_lookup(TypeParamMap *map, const char *param_name) {
    if (map == NULL || param_name == NULL) {
        return NULL;
    }
    
    for (int i = 0; i < map->count; i++) {
        if (strcmp(map->bindings[i].param_name, param_name) == 0) {
            return map->bindings[i].actual_type;
        }
    }
    
    return NULL;
}

/*
** 类型替换：将类型中的类型参数替换为实际类型
** 用于泛型实例化
** 例如：identity<T>(x: T): T 实例化为 identity<int>
**      T → int
*/
XrTypeInfo* xr_type_substitute(XrayState *X, XrTypeInfo *type, TypeParamMap *map) {
    if (type == NULL || map == NULL) {
        return type;
    }
    
    switch (type->kind) {
        case TYPE_PARAM: {
            /* 类型参数：查找映射 */
            XrTypeInfo *actual = xr_type_param_map_lookup(map, type->as.type_param.name);
            return actual ? actual : type;
        }
        
        case TYPE_ARRAY: {
            /* 数组类型：递归替换元素类型 */
            XrTypeInfo *elem = xr_type_substitute(X, type->as.array.element_type, map);
            if (elem != type->as.array.element_type) {
                return xr_type_array(X, elem);
            }
            return type;
        }
        
        case TYPE_MAP: {
            /* Map类型：递归替换键和值类型 */
            XrTypeInfo *key = xr_type_substitute(X, type->as.map.key_type, map);
            XrTypeInfo *val = xr_type_substitute(X, type->as.map.value_type, map);
            if (key != type->as.map.key_type || val != type->as.map.value_type) {
                return xr_type_map(X, key, val);
            }
            return type;
        }
        
        case TYPE_FUNCTION: {
            /* 函数类型：递归替换参数和返回类型 */
            bool changed = false;
            XrTypeInfo **new_params = NULL;
            
            if (type->as.function.param_count > 0) {
                new_params = (XrTypeInfo**)malloc(sizeof(XrTypeInfo*) * type->as.function.param_count);
                for (int i = 0; i < type->as.function.param_count; i++) {
                    new_params[i] = xr_type_substitute(X, type->as.function.param_types[i], map);
                    if (new_params[i] != type->as.function.param_types[i]) {
                        changed = true;
                    }
                }
            }
            
            XrTypeInfo *ret = xr_type_substitute(X, type->as.function.return_type, map);
            if (ret != type->as.function.return_type) {
                changed = true;
            }
            
            if (changed) {
                XrTypeInfo *result = xr_type_function(X, new_params, 
                                                     type->as.function.param_count, ret);
                if (new_params) free(new_params);
                return result;
            }
            
            if (new_params) free(new_params);
            return type;
        }
        
        case TYPE_UNION: {
            /* 联合类型：递归替换所有成员类型 */
            bool changed = false;
            XrTypeInfo **new_types = (XrTypeInfo**)malloc(sizeof(XrTypeInfo*) * type->as.union_type.type_count);
            
            for (int i = 0; i < type->as.union_type.type_count; i++) {
                new_types[i] = xr_type_substitute(X, type->as.union_type.types[i], map);
                if (new_types[i] != type->as.union_type.types[i]) {
                    changed = true;
                }
            }
            
            if (changed) {
                XrTypeInfo *result = xr_type_union(X, new_types, type->as.union_type.type_count);
                free(new_types);
                return result;
            }
            
            free(new_types);
            return type;
        }
        
        default:
            /* 基本类型：不需要替换 */
            return type;
    }
}

/* ========== 类型系统初始化 ========== */

/*
** 初始化内置类型
*/
void xr_type_init(XrayState *X) {
    (void)X;  /* 暂未使用 */
    /* 内置类型是全局静态变量，已自动初始化 */
}


/*
** xclass.c
** Xray 类对象实现
** 
** v0.12.0：第十二阶段 - OOP系统
*/

#include "xclass.h"
#include "xinstance.h"
#include "xmethod.h"
#include "xmem.h"
#include "xobject.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* ========== 内部辅助函数 ========== */

/*
** 复制字符串
*/
static char* str_dup(const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char *copy = (char*)xmem_alloc(len + 1);
    memcpy(copy, str, len + 1);
    return copy;
}

/* ========== 类对象操作实现 ========== */

/*
** 创建新类
*/
XrClass* xr_class_new(XrayState *X, const char *name, XrClass *super) {
    assert(name != NULL);
    
    /* 分配类对象内存 */
    XrClass *cls = XR_ALLOCATE(XrClass, OBJ_CLASS);
    
    /* 初始化XrObject头部 */
    cls->header.type = XR_TCLASS;
    cls->header.type_info = NULL;
    cls->header.marked = false;
    cls->header.next = NULL;
    
    /* 初始化基本字段 */
    cls->name = str_dup(name);
    cls->super = super;
    cls->type_info = NULL;  /* 稍后通过类型系统设置 */
    
    /* 初始化字段定义 */
    cls->field_names = NULL;
    cls->field_types = NULL;
    cls->field_count = 0;
    cls->own_field_count = 0;
    
    /* v0.20.0: 初始化方法数组 */
    cls->methods = NULL;       /* 方法数组（按需分配）*/
    cls->method_count = 0;
    
    /* 静态方法（保持哈希表）*/
    cls->static_methods = xr_hashmap_new();
    
    /* v0.20.0: 初始化备用哈希表（向后兼容）*/
    cls->methods_map = xr_hashmap_new();
    
    /* 创建访问控制表 */
    cls->private_fields = xr_hashmap_new();
    cls->private_methods = xr_hashmap_new();
    
    /* 创建静态字段表 */
    cls->static_fields = xr_hashmap_new();
    
    /* 元类暂不使用 */
    cls->metaclass = NULL;
    
    /* 如果有超类，继承字段 */
    if (super) {
        xr_class_set_super(cls, super);
    }
    
    return cls;
}

/*
** 释放类对象
*/
void xr_class_free(XrClass *cls) {
    if (!cls) return;
    
    /* 释放类名 */
    if (cls->name) {
        xmem_free(cls->name);
    }
    
    /* 释放字段定义 */
    if (cls->field_names) {
        for (int i = 0; i < cls->own_field_count; i++) {
            xmem_free(cls->field_names[cls->field_count - cls->own_field_count + i]);
        }
        xmem_free(cls->field_names);
    }
    if (cls->field_types) {
        xmem_free(cls->field_types);
    }
    
    /* v0.20.0: 释放方法数组 */
    if (cls->methods) {
        xmem_free(cls->methods);
    }
    
    /* 释放哈希表 */
    if (cls->methods_map) {
        xr_hashmap_free(cls->methods_map);
    }
    xr_hashmap_free(cls->static_methods);
    xr_hashmap_free(cls->private_fields);
    xr_hashmap_free(cls->private_methods);
    xr_hashmap_free(cls->static_fields);
    
    /* 释放类对象本身 */
    xmem_free(cls);
}

/*
** 设置超类（建立继承关系）
*/
void xr_class_set_super(XrClass *subclass, XrClass *superclass) {
    if (!subclass || !superclass) return;
    
    subclass->super = superclass;
    
    /* 继承字段 */
    int super_field_count = superclass->field_count;
    int own_count = subclass->own_field_count;
    int new_count = super_field_count + own_count;
    
    /* 分配新的字段数组 */
    char **new_names = (char**)xmem_alloc(sizeof(char*) * new_count);
    XrTypeInfo **new_types = (XrTypeInfo**)xmem_alloc(sizeof(XrTypeInfo*) * new_count);
    
    /* 复制超类字段（前面）*/
    for (int i = 0; i < super_field_count; i++) {
        new_names[i] = superclass->field_names[i];
        new_types[i] = superclass->field_types[i];
    }
    
    /* 复制本类字段（后面）*/
    if (subclass->field_names) {
        for (int i = 0; i < own_count; i++) {
            new_names[super_field_count + i] = subclass->field_names[i];
            new_types[super_field_count + i] = subclass->field_types[i];
        }
        xmem_free(subclass->field_names);
        xmem_free(subclass->field_types);
    }
    
    subclass->field_names = new_names;
    subclass->field_types = new_types;
    subclass->field_count = new_count;
}

/*
** 添加字段定义
*/
void xr_class_add_field(XrClass *cls, const char *name, XrTypeInfo *type) {
    assert(cls != NULL);
    assert(name != NULL);
    
    int new_count = cls->field_count + 1;
    
    /* 扩展字段数组 */
    cls->field_names = (char**)xmem_realloc(cls->field_names, 
                                             sizeof(char*) * cls->field_count,
                                             sizeof(char*) * new_count);
    cls->field_types = (XrTypeInfo**)xmem_realloc(cls->field_types, 
                                                    sizeof(XrTypeInfo*) * cls->field_count,
                                                    sizeof(XrTypeInfo*) * new_count);
    
    /* 添加新字段 */
    cls->field_names[cls->field_count] = str_dup(name);
    cls->field_types[cls->field_count] = type;
    cls->field_count = new_count;
    cls->own_field_count++;
}

/*
** 查找字段索引
*/
int xr_class_find_field_index(XrClass *cls, const char *name) {
    if (!cls || !name) return -1;
    
    for (int i = 0; i < cls->field_count; i++) {
        if (strcmp(cls->field_names[i], name) == 0) {
            return i;
        }
    }
    
    return -1;  /* 未找到 */
}

/*
** v0.20.0: 通过Symbol添加方法（高性能版本）
*/
void xr_class_add_method_by_symbol(XrClass *cls, int symbol, XrMethod *method) {
    assert(cls != NULL);
    assert(method != NULL);
    assert(symbol >= 0);
    
    /* 扩展方法数组（如果需要）*/
    if (symbol >= cls->method_count) {
        int new_count = symbol + 1;
        
        /* 重新分配数组 */
        XrMethod **new_methods = (XrMethod**)xmem_alloc(sizeof(XrMethod*) * new_count);
        
        /* 复制旧数据 */
        for (int i = 0; i < cls->method_count; i++) {
            new_methods[i] = cls->methods[i];
        }
        
        /* 填充NULL */
        for (int i = cls->method_count; i < new_count; i++) {
            new_methods[i] = NULL;
        }
        
        /* 释放旧数组并使用新数组 */
        if (cls->methods) {
            xmem_free(cls->methods);
        }
        cls->methods = new_methods;
        cls->method_count = new_count;
    }
    
    /* 设置方法 */
    cls->methods[symbol] = method;
    
    /* 同时添加到哈希表（向后兼容）*/
    xr_hashmap_set(cls->methods_map, method->name, method);
}

/*
** 添加方法（向后兼容接口）
*/
void xr_class_add_method(XrClass *cls, XrMethod *method) {
    assert(cls != NULL);
    assert(method != NULL);
    
    /* v0.20.0: 保持向后兼容，仍使用哈希表 */
    xr_hashmap_set(cls->methods_map, method->name, method);
}

/*
** v0.20.0: 通过Symbol查找方法（高性能版本）
*/
XrMethod* xr_class_lookup_method_by_symbol(XrClass *cls, int symbol) {
    if (!cls || symbol < 0) return NULL;
    
    /* 1. 当前类查找（O(1)数组访问）*/
    if (symbol < cls->method_count && cls->methods[symbol]) {
        return cls->methods[symbol];
    }
    
    /* 2. 超类递归查找（支持继承）*/
    if (cls->super) {
        return xr_class_lookup_method_by_symbol(cls->super, symbol);
    }
    
    /* 3. 未找到 */
    return NULL;
}

/*
** 查找方法（向后兼容接口）
*/
XrMethod* xr_class_lookup_method(XrClass *cls, const char *name) {
    if (!cls || !name) return NULL;
    
    /* v0.20.0: 使用哈希表查找（向后兼容）*/
    XrMethod *method = (XrMethod*)xr_hashmap_get(cls->methods_map, name);
    if (method) {
        return method;
    }
    
    /* 超类递归查找（支持继承）*/
    if (cls->super) {
        return xr_class_lookup_method(cls->super, name);
    }
    
    /* 未找到 */
    return NULL;
}

/*
** 添加静态方法
*/
void xr_class_add_static_method(XrClass *cls, XrMethod *method) {
    assert(cls != NULL);
    assert(method != NULL);
    
    method->is_static = true;
    xr_hashmap_set(cls->static_methods, method->name, method);
}

/*
** 查找静态方法
*/
XrMethod* xr_class_lookup_static_method(XrClass *cls, const char *name) {
    if (!cls || !name) return NULL;
    
    /* 静态方法不继承，只在当前类查找 */
    return (XrMethod*)xr_hashmap_get(cls->static_methods, name);
}

/*
** 添加静态字段
*/
void xr_class_add_static_field(XrClass *cls, const char *name, XrValue value) {
    assert(cls != NULL);
    assert(name != NULL);
    
    /* 复制值到堆上 */
    XrValue *val_copy = (XrValue*)xmem_alloc(sizeof(XrValue));
    *val_copy = value;
    
    xr_hashmap_set(cls->static_fields, name, val_copy);
}

/*
** 获取静态字段值
*/
XrValue xr_class_get_static_field(XrClass *cls, const char *name) {
    if (!cls || !name) return xr_null();
    
    XrValue *ptr = (XrValue*)xr_hashmap_get(cls->static_fields, name);
    if (!ptr) {
        return xr_null();
    }
    
    return *ptr;
}

/* ========== 访问控制实现 ========== */

/*
** 标记字段为私有
*/
void xr_class_mark_field_private(XrClass *cls, const char *field_name) {
    assert(cls != NULL);
    assert(field_name != NULL);
    
    /* 添加到私有字段集合 */
    xr_hashmap_set(cls->private_fields, field_name, (void*)1);
}

/*
** 检查字段是否为私有
*/
bool xr_class_is_field_private(XrClass *cls, const char *field_name) {
    if (!cls || !field_name) return false;
    
    void *result = xr_hashmap_get(cls->private_fields, field_name);
    return result != NULL;
}

/*
** 检查是否可以访问字段
*/
bool xr_class_can_access_field(XrClass *cls, const char *field_name,
                                XrClass *accessor_class) {
    if (!cls || !field_name) return false;
    
    /* 1. 公有字段：所有类可访问 */
    if (!xr_class_is_field_private(cls, field_name)) {
        return true;
    }
    
    /* 2. 私有字段：仅定义类和子类可访问 */
    XrClass *current = accessor_class;
    while (current) {
        if (current == cls) return true;
        current = current->super;
    }
    
    return false;
}

/*
** 标记方法为私有
*/
void xr_class_mark_method_private(XrClass *cls, const char *method_name) {
    assert(cls != NULL);
    assert(method_name != NULL);
    
    /* 添加到私有方法集合 */
    xr_hashmap_set(cls->private_methods, method_name, (void*)1);
}

/*
** 检查方法是否为私有
*/
bool xr_class_is_method_private(XrClass *cls, const char *method_name) {
    if (!cls || !method_name) return false;
    
    void *result = xr_hashmap_get(cls->private_methods, method_name);
    return result != NULL;
}

/* ========== 辅助函数实现 ========== */

/*
** 打印类信息（调试用）
*/
void xr_class_print(XrClass *cls) {
    if (!cls) {
        printf("null class\n");
        return;
    }
    
    printf("Class %s", cls->name);
    if (cls->super) {
        printf(" extends %s", cls->super->name);
    }
    printf(" {\n");
    
    /* 打印字段 */
    printf("  Fields (%d):\n", cls->field_count);
    for (int i = 0; i < cls->field_count; i++) {
        printf("    [%d] %s", i, cls->field_names[i]);
        if (xr_class_is_field_private(cls, cls->field_names[i])) {
            printf(" (private)");
        }
        printf("\n");
    }
    
    /* v0.20.0: 打印方法数量（methods现在是数组）*/
    int method_count = 0;
    for (int i = 0; i < cls->method_count; i++) {
        if (cls->methods[i]) method_count++;
    }
    printf("  Methods: %d (array size: %d)\n", method_count, cls->method_count);
    printf("  Static methods: %d\n", cls->static_methods->count);
    
    printf("}\n");
}


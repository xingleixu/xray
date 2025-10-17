/*
** xclass.h
** Xray 类对象定义
** 
** v0.12.0：第十二阶段 - OOP系统
** 参考：Wren的纯类设计
*/

#ifndef xclass_h
#define xclass_h

#include "xray.h"
#include "xvalue.h"
#include "xtype.h"
#include "xhashmap.h"
#include <stdbool.h>

/* 前向声明 */
typedef struct XrMethod XrMethod;
typedef struct XrClass XrClass;
typedef struct XrInstance XrInstance;

/*
** 类对象
** 
** 设计说明：
** 1. 单继承设计（参考Wren）：简单清晰，super指针指向父类
** 2. 字段继承：field_count = own_field_count + super->field_count
** 3. 方法表：初期使用哈希表，后期可优化为直接索引
** 4. 访问控制：使用独立哈希表标记私有成员
*/
struct XrClass {
    XrObject header;           /* GC头部（统一对象接口）*/
    
    /* 基本信息 */
    char *name;                /* 类名："Person" */
    struct XrClass *super;     /* 超类指针（单继承，NULL表示无父类）*/
    XrTypeInfo *type_info;     /* 类型信息（与类型系统集成）*/
    
    /* 字段定义 */
    char **field_names;        /* 字段名数组：["name", "age"] */
    XrTypeInfo **field_types;  /* 字段类型数组：[string, int] */
    int field_count;           /* 字段总数（包括继承的）*/
    int own_field_count;       /* 本类定义的字段数量（不含继承）*/
    
    /* 方法表 */
    XrHashMap *methods;        /* 实例方法表：name → XrMethod */
    XrHashMap *static_methods; /* 静态方法表：name → XrMethod */
    
    /* 访问控制（Day 12-14实现）*/
    XrHashMap *private_fields; /* 私有字段集合：name → true */
    XrHashMap *private_methods;/* 私有方法集合：name → true */
    
    /* 静态字段（Day 15-16实现）*/
    XrHashMap *static_fields;  /* 静态字段：name → XrValue */
    
    /* 元类（可选，第13阶段扩展）*/
    struct XrClass *metaclass; /* 元类指针（当前为NULL）*/
};

/* ========== 类对象操作 ========== */

/*
** 创建新类
** 
** @param X         Xray状态机
** @param name      类名
** @param super     超类（NULL表示无父类）
** @return          新创建的类对象
*/
XrClass* xr_class_new(XrayState *X, const char *name, XrClass *super);

/*
** 释放类对象
** 
** @param cls       类对象
*/
void xr_class_free(XrClass *cls);

/*
** 设置超类（建立继承关系）
** 
** @param subclass  子类
** @param superclass 超类
*/
void xr_class_set_super(XrClass *subclass, XrClass *superclass);

/*
** 添加字段定义
** 
** @param cls       类对象
** @param name      字段名
** @param type      字段类型
*/
void xr_class_add_field(XrClass *cls, const char *name, XrTypeInfo *type);

/*
** 查找字段索引
** 
** @param cls       类对象
** @param name      字段名
** @return          字段索引（-1表示未找到）
*/
int xr_class_find_field_index(XrClass *cls, const char *name);

/*
** 添加方法
** 
** @param cls       类对象
** @param method    方法对象
*/
void xr_class_add_method(XrClass *cls, XrMethod *method);

/*
** 查找方法（支持继承链查找）
** 
** @param cls       类对象
** @param name      方法名
** @return          方法对象（NULL表示未找到）
*/
XrMethod* xr_class_lookup_method(XrClass *cls, const char *name);

/*
** 添加静态方法（Day 15-16实现）
** 
** @param cls       类对象
** @param method    方法对象
*/
void xr_class_add_static_method(XrClass *cls, XrMethod *method);

/*
** 查找静态方法
** 
** @param cls       类对象
** @param name      方法名
** @return          方法对象（NULL表示未找到）
*/
XrMethod* xr_class_lookup_static_method(XrClass *cls, const char *name);

/*
** 添加静态字段（Day 15-16实现）
** 
** @param cls       类对象
** @param name      字段名
** @param value     字段值
*/
void xr_class_add_static_field(XrClass *cls, const char *name, XrValue value);

/*
** 获取静态字段值
** 
** @param cls       类对象
** @param name      字段名
** @return          字段值
*/
XrValue xr_class_get_static_field(XrClass *cls, const char *name);

/* ========== 访问控制（Day 12-14实现）========== */

/*
** 标记字段为私有
** 
** @param cls       类对象
** @param field_name 字段名
*/
void xr_class_mark_field_private(XrClass *cls, const char *field_name);

/*
** 检查字段是否为私有
** 
** @param cls       类对象
** @param field_name 字段名
** @return          true表示私有
*/
bool xr_class_is_field_private(XrClass *cls, const char *field_name);

/*
** 检查是否可以访问字段
** 
** @param cls           字段所在类
** @param field_name    字段名
** @param accessor_class 访问者类
** @return              true表示可访问
*/
bool xr_class_can_access_field(XrClass *cls, const char *field_name,
                                XrClass *accessor_class);

/*
** 标记方法为私有
** 
** @param cls       类对象
** @param method_name 方法名
*/
void xr_class_mark_method_private(XrClass *cls, const char *method_name);

/*
** 检查方法是否为私有
** 
** @param cls       类对象
** @param method_name 方法名
** @return          true表示私有
*/
bool xr_class_is_method_private(XrClass *cls, const char *method_name);

/* ========== 辅助函数 ========== */

/*
** 打印类信息（调试用）
** 
** @param cls       类对象
*/
void xr_class_print(XrClass *cls);

#endif /* xclass_h */


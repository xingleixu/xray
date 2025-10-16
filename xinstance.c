/*
** xinstance.c
** Xray 实例对象实现
** 
** v0.12.0：第十二阶段 - OOP系统
*/

#include "xinstance.h"
#include "xclass.h"
#include "xmethod.h"
#include "xmem.h"
#include "xobject.h"
#include "xeval.h"
#include "xstring.h"  /* 需要完整的XrString定义 */
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* ========== 实例对象操作实现 ========== */

/*
** 创建新实例
*/
XrInstance* xr_instance_new(XrayState *X, XrClass *cls) {
    assert(cls != NULL);
    
    /* 分配实例内存：头部 + 字段数组 */
    size_t size = sizeof(XrInstance) + sizeof(XrValue) * cls->field_count;
    XrInstance *inst = (XrInstance*)gc_alloc(size, OBJ_INSTANCE);
    
    /* 初始化对象头（XrObject不包含嵌套的gc字段）*/
    xr_object_init(&inst->header, XR_TINSTANCE, NULL);
    
    /* 设置所属类 */
    inst->klass = cls;
    
    /* 初始化所有字段为null */
    for (int i = 0; i < cls->field_count; i++) {
        inst->fields[i] = xr_null();
    }
    
    return inst;
}

/*
** 释放实例对象
*/
void xr_instance_free(XrInstance *inst) {
    if (!inst) return;
    
    /* 字段值释放由GC管理，这里只释放实例本身 */
    xmem_free(inst);
}

/*
** 获取字段值
** 
** v0.12.1: 简化访问控制策略
** - 如果是通过索引访问（内部）：允许
** - 如果通过名称访问私有字段：禁止（外部访问）
** - 实际上this.field会先通过符号表找到this，然后调用本函数
** - 所以暂时不在这里检查，由上层控制
*/
XrValue xr_instance_get_field(XrInstance *inst, const char *name) {
    if (!inst || !name) return xr_null();
    
    /* 查找字段索引 */
    int index = xr_class_find_field_index(inst->klass, name);
    if (index < 0) {
        fprintf(stderr, "Runtime Error: Field '%s' not found in class '%s'\n",
                name, inst->klass->name);
        return xr_null();
    }
    
    /* v0.12.1: 访问控制暂时简化 - 允许访问（通过this访问时）*/
    /* 更复杂的检查需要传递"访问者类"上下文 */
    
    return inst->fields[index];
}

/*
** 设置字段值
** 
** v0.12.1: 简化访问控制策略
** - 暂时允许所有访问（通过this访问时）
** - 更复杂的检查需要上下文
*/
void xr_instance_set_field(XrInstance *inst, const char *name, XrValue value) {
    if (!inst || !name) return;
    
    /* 查找字段索引 */
    int index = xr_class_find_field_index(inst->klass, name);
    if (index < 0) {
        fprintf(stderr, "Runtime Error: Field '%s' not found in class '%s'\n",
                name, inst->klass->name);
        return;
    }
    
    /* v0.12.1: 访问控制暂时简化 - 允许访问（通过this访问时）*/
    
    /* 类型检查（运行时）*/
    XrTypeInfo *expected = inst->klass->field_types[index];
    if (expected && !xr_value_is_type(&value, expected)) {
        fprintf(stderr, "Runtime Error: Type mismatch for field '%s'\n", name);
        return;
    }
    
    inst->fields[index] = value;
}

/*
** 通过索引获取字段值（快速路径）
*/
XrValue xr_instance_get_field_by_index(XrInstance *inst, int index) {
    assert(inst != NULL);
    assert(index >= 0 && index < inst->klass->field_count);
    
    return inst->fields[index];
}

/*
** 通过索引设置字段值（快速路径）
*/
void xr_instance_set_field_by_index(XrInstance *inst, int index, XrValue value) {
    assert(inst != NULL);
    assert(index >= 0 && index < inst->klass->field_count);
    
    inst->fields[index] = value;
}

/*
** 调用实例方法
*/
XrValue xr_instance_call_method(XrayState *X, XrInstance *inst, 
                                 const char *name, 
                                 XrValue *args, int argc,
                                 XSymbolTable *symbols) {
    if (!X || !inst || !name) return xr_null();
    
    /* 1. 查找方法（支持继承链）*/
    XrMethod *method = xr_class_lookup_method(inst->klass, name);
    if (!method) {
        fprintf(stderr, "Runtime Error: Method '%s' not found in class '%s'\n",
                name, inst->klass->name);
        return xr_null();
    }
    
    /* 2. v0.12.1: 访问权限检查 */
    /* 暂时简化：允许访问（通过this访问时）*/
    /* 更复杂的检查需要上下文 */
    
    /* 3. 绑定 this */
    XrValue this_value = xr_value_from_instance(inst);
    
    /* 4. 准备参数：[this, arg1, arg2, ...] */
    XrValue *full_args = (XrValue*)xmem_alloc(sizeof(XrValue) * (argc + 1));
    full_args[0] = this_value;
    for (int i = 0; i < argc; i++) {
        full_args[i + 1] = args[i];
    }
    
    /* 5. 调用底层函数 */
    /* TODO: 使用字节码VM重新实现 */
    fprintf(stderr, "实例方法调用: 暂未实现（等待字节码VM支持）\n");
    XrValue result = xr_null();
    
    xmem_free(full_args);
    return result;
}

/*
** 构造实例（调用constructor）
*/
XrValue xr_instance_construct(XrayState *X, XrClass *cls, 
                               XrValue *args, int argc,
                               XSymbolTable *symbols) {
    if (!X || !cls) return xr_null();
    
    /* 1. 分配实例内存 */
    XrInstance *inst = xr_instance_new(X, cls);
    
    /* 2. 初始化所有字段为null（已在xr_instance_new中完成）*/
    
    /* 3. 查找constructor方法 */
    XrMethod *ctor = xr_class_lookup_method(cls, "constructor");
    if (!ctor) {
        /* 无构造函数 → 使用默认构造 */
        return xr_value_from_instance(inst);
    }
    
    /* 4. 调用构造函数 */
    XrValue this_value = xr_value_from_instance(inst);
    XrValue *full_args = (XrValue*)xmem_alloc(sizeof(XrValue) * (argc + 1));
    full_args[0] = this_value;  /* 第一个参数是 this */
    for (int i = 0; i < argc; i++) {
        full_args[i + 1] = args[i];
    }
    
    /* TODO: 使用字节码VM重新实现 */
    fprintf(stderr, "构造函数调用: 暂未实现（等待字节码VM支持）\n");
    (void)ctor;  /* 抑制未使用警告 */
    xmem_free(full_args);
    
    return this_value;
}

/* ========== 辅助函数实现 ========== */

/*
** 打印实例信息（调试用）
*/
void xr_instance_print(XrInstance *inst) {
    if (!inst) {
        printf("null instance\n");
        return;
    }
    
    printf("%s instance {\n", inst->klass->name);
    
    /* 打印字段值 */
    for (int i = 0; i < inst->klass->field_count; i++) {
        printf("  %s: ", inst->klass->field_names[i]);
        
        XrValue val = inst->fields[i];
        if (xr_isnull(val)) {
            printf("null");
        } else if (xr_isbool(val)) {
            printf("%s", xr_tobool(val) ? "true" : "false");
        } else if (xr_isint(val)) {
            printf("%lld", xr_toint(val));
        } else if (xr_isfloat(val)) {
            printf("%g", xr_tofloat(val));
        } else if (xr_isstring(val)) {
            printf("\"%s\"", xr_tostring(val)->chars);
        } else {
            printf("<object>");
        }
        printf("\n");
    }
    
    printf("}\n");
}

/*
** 检查实例是否属于某个类（支持继承检查）
*/
bool xr_instance_is_a(XrInstance *inst, XrClass *cls) {
    if (!inst || !cls) return false;
    
    XrClass *current = inst->klass;
    while (current) {
        if (current == cls) return true;
        current = current->super;
    }
    
    return false;
}


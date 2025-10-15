/*
** xinstance.h
** Xray 实例对象定义
** 
** v0.12.0：第十二阶段 - OOP系统
*/

#ifndef xinstance_h
#define xinstance_h

#include "xray.h"
#include "xvalue.h"
#include "xclass.h"
#include "xscope.h"  /* 需要XSymbolTable完整定义 */

/*
** 实例对象
** 
** 设计说明：
** 1. 柔性数组成员存储字段值
** 2. 字段数组大小由类的 field_count 决定
** 3. 字段顺序：超类字段在前，本类字段在后
** 
** 内存布局示例（Person类有2个字段：name, age）：
** ┌─────────────────┐
** │ header (GC信息) │  ← XrObject (24字节)
** ├─────────────────┤
** │ klass 指针      │  ← 8字节
** ├─────────────────┤
** │ fields[0] (name)│  ← XrValue (16字节，Tagged Union模式)
** ├─────────────────┤
** │ fields[1] (age) │  ← XrValue (16字节)
** └─────────────────┘
** 总大小：24 + 8 + 16*2 = 56字节
** 
** NaN Tagging模式（第13阶段）：
** 总大小：24 + 8 + 8*2 = 48字节（减少14%）
*/
struct XrInstance {
    XrObject header;           /* GC头部 */
    XrClass *klass;            /* 所属类 */
    XrValue fields[];          /* 字段值数组（柔性数组成员）*/
};

/* ========== 实例对象操作 ========== */

/*
** 创建新实例
** 
** @param X         Xray状态机
** @param cls       所属类
** @return          新创建的实例对象
** 
** 注意：
** - 所有字段初始化为null
** - 不调用构造函数（由eval层负责）
*/
XrInstance* xr_instance_new(XrayState *X, XrClass *cls);

/*
** 释放实例对象
** 
** @param inst      实例对象
*/
void xr_instance_free(XrInstance *inst);

/*
** 获取字段值
** 
** @param inst      实例对象
** @param name      字段名
** @return          字段值（未找到返回null）
*/
XrValue xr_instance_get_field(XrInstance *inst, const char *name);

/*
** 设置字段值
** 
** @param inst      实例对象
** @param name      字段名
** @param value     字段值
** 
** 注意：会进行类型检查（运行时）
*/
void xr_instance_set_field(XrInstance *inst, const char *name, XrValue value);

/*
** 通过索引获取字段值（快速路径）
** 
** @param inst      实例对象
** @param index     字段索引
** @return          字段值
*/
XrValue xr_instance_get_field_by_index(XrInstance *inst, int index);

/*
** 通过索引设置字段值（快速路径）
** 
** @param inst      实例对象
** @param index     字段索引
** @param value     字段值
*/
void xr_instance_set_field_by_index(XrInstance *inst, int index, XrValue value);

/*
** 调用实例方法
** 
** @param X         Xray状态机
** @param inst      实例对象
** @param name      方法名
** @param args      参数数组
** @param argc      参数数量
** @param symbols   父符号表（可为NULL）
** @return          返回值
*/
XrValue xr_instance_call_method(XrayState *X, XrInstance *inst, 
                                 const char *name, 
                                 XrValue *args, int argc,
                                 XSymbolTable *symbols);

/*
** 构造实例（调用constructor）
** 
** @param X         Xray状态机
** @param cls       类对象
** @param args      构造参数
** @param argc      参数数量
** @param symbols   父符号表（可为NULL）
** @return          构造好的实例
** 
** 流程：
** 1. 分配实例内存
** 2. 初始化所有字段为null
** 3. 查找constructor方法
** 4. 调用constructor（绑定this）
** 5. 返回实例
*/
XrValue xr_instance_construct(XrayState *X, XrClass *cls, 
                               XrValue *args, int argc,
                               XSymbolTable *symbols);

/* ========== 辅助函数 ========== */

/*
** 打印实例信息（调试用）
** 
** @param inst      实例对象
*/
void xr_instance_print(XrInstance *inst);

/*
** 检查实例是否属于某个类（支持继承检查）
** 
** @param inst      实例对象
** @param cls       类对象
** @return          true表示是该类的实例
*/
bool xr_instance_is_a(XrInstance *inst, XrClass *cls);

#endif /* xinstance_h */


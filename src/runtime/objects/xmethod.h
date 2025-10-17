/*
** xmethod.h
** Xray 方法对象定义
** 
** v0.12.0：第十二阶段 - OOP系统
*/

#ifndef xmethod_h
#define xmethod_h

#include "xray.h"
#include "xvalue.h"
#include "xscope.h"  /* 需要XSymbolTable完整定义 */
#include <stdbool.h>

/* 前向声明运算符类型（v0.19.0）*/
#include "xast.h"  /* 需要 OperatorType */

/*
** 方法对象
** 
** 设计说明：
** 1. 方法是函数的包装，增加了OOP相关属性
** 2. 实例方法：第一个参数自动绑定this
** 3. 静态方法：无this绑定
** 4. Getter/Setter：特殊标记，语法糖支持
** 5. v0.19.0：支持运算符重载
*/
typedef struct XrMethod {
    XrObject header;           /* GC头部 */
    
    char *name;                /* 方法名："greet" 或运算符符号："+" */
    XrFunction *func;          /* 底层函数对象 */
    
    /* 方法属性 */
    bool is_static;            /* 是否静态方法 */
    bool is_private;           /* 是否私有方法 */
    bool is_constructor;       /* 是否构造函数 */
    
    /* Getter/Setter标记（Day 17-19实现）*/
    bool is_getter;            /* get propertyName() { ... } */
    bool is_setter;            /* set propertyName(value) { ... } */
    
    /* v0.19.0新增：运算符重载相关 */
    bool is_operator;          /* 是否为运算符重载方法 */
    OperatorType op_type;      /* 运算符类型 */
} XrMethod;

/* ========== 方法对象操作 ========== */

/*
** 创建新方法
** 
** @param X         Xray状态机
** @param name      方法名
** @param func      底层函数
** @param is_static 是否静态方法
** @return          新创建的方法对象
*/
XrMethod* xr_method_new(XrayState *X, const char *name, 
                        XrFunction *func, bool is_static);

/*
** 释放方法对象
** 
** @param method    方法对象
*/
void xr_method_free(XrMethod *method);

/*
** 设置方法为构造函数
** 
** @param method    方法对象
*/
void xr_method_mark_constructor(XrMethod *method);

/*
** 设置方法为私有
** 
** @param method    方法对象
*/
void xr_method_mark_private(XrMethod *method);

/*
** 设置方法为Getter
** 
** @param method    方法对象
*/
void xr_method_mark_getter(XrMethod *method);

/*
** 设置方法为Setter
** 
** @param method    方法对象
*/
void xr_method_mark_setter(XrMethod *method);

/*
** 设置方法为运算符重载（v0.19.0新增）
** 
** @param method    方法对象
** @param op_type   运算符类型
*/
void xr_method_mark_operator(XrMethod *method, OperatorType op_type);

/*
** 调用方法（绑定this）
** 
** @param X         Xray状态机
** @param method    方法对象
** @param this_val  this值（实例对象）
** @param args      参数数组
** @param argc      参数数量
** @param symbols   父符号表（可为NULL）
** @return          返回值
*/
XrValue xr_method_call(XrayState *X, XrMethod *method, 
                       XrValue this_val, 
                       XrValue *args, int argc,
                       XSymbolTable *symbols);

/*
** 调用静态方法（无this绑定）
** 
** @param X         Xray状态机
** @param method    方法对象
** @param args      参数数组
** @param argc      参数数量
** @param symbols   父符号表（可为NULL）
** @return          返回值
*/
XrValue xr_method_call_static(XrayState *X, XrMethod *method, 
                               XrValue *args, int argc,
                               XSymbolTable *symbols);

/* ========== 辅助函数 ========== */

/*
** 打印方法信息（调试用）
** 
** @param method    方法对象
*/
void xr_method_print(XrMethod *method);

/*
** 获取方法签名字符串
** 
** @param method    方法对象
** @return          签名字符串（需要释放）
** 
** 示例：
** - 实例方法："greet(name: string): void"
** - 静态方法："static abs(x: float): float"
** - Getter："get width(): float"
** - Setter："set width(value: float)"
*/
char* xr_method_signature(XrMethod *method);

#endif /* xmethod_h */


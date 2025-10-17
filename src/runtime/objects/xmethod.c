/*
** xmethod.c
** Xray 方法对象实现
** 
** v0.12.0：第十二阶段 - OOP系统
*/

#include "xmethod.h"
#include "xvalue.h"
#include "xtype.h"   /* 需要xr_type_kind_name和XrTypeInfo定义 */
#include "xmem.h"
#include "xobject.h"
/* xeval.h 已废弃，使用字节码VM */
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

/* ========== 方法对象操作实现 ========== */

/*
** 创建新方法
*/
XrMethod* xr_method_new(XrayState *X, const char *name, 
                        XrFunction *func, bool is_static) {
    assert(name != NULL);
    assert(func != NULL);
    
    /* 分配方法对象内存 */
    XrMethod *method = XR_ALLOCATE(XrMethod, OBJ_METHOD);
    
    /* 初始化字段 */
    method->name = str_dup(name);
    method->func = func;
    method->is_static = is_static;
    method->is_private = false;
    method->is_constructor = false;
    method->is_getter = false;
    method->is_setter = false;
    
    /* v0.19.0：初始化运算符相关字段 */
    method->is_operator = false;
    method->op_type = OP_BINARY;  /* 默认值 */
    
    return method;
}

/*
** 释放方法对象
*/
void xr_method_free(XrMethod *method) {
    if (!method) return;
    
    /* 释放方法名 */
    if (method->name) {
        xmem_free(method->name);
    }
    
    /* 函数对象由GC管理，不在这里释放 */
    
    /* 释放方法对象本身 */
    xmem_free(method);
}

/*
** 设置方法为构造函数
*/
void xr_method_mark_constructor(XrMethod *method) {
    assert(method != NULL);
    method->is_constructor = true;
}

/*
** 设置方法为私有
*/
void xr_method_mark_private(XrMethod *method) {
    assert(method != NULL);
    method->is_private = true;
}

/*
** 设置方法为Getter
*/
void xr_method_mark_getter(XrMethod *method) {
    assert(method != NULL);
    method->is_getter = true;
}

/*
** 设置方法为Setter
*/
void xr_method_mark_setter(XrMethod *method) {
    assert(method != NULL);
    method->is_setter = true;
}

/*
** 设置方法为运算符重载（v0.19.0新增）
*/
void xr_method_mark_operator(XrMethod *method, OperatorType op_type) {
    assert(method != NULL);
    method->is_operator = true;
    method->op_type = op_type;
}

/*
** 调用方法（绑定this）
*/
XrValue xr_method_call(XrayState *X, XrMethod *method, 
                       XrValue this_val, 
                       XrValue *args, int argc,
                       XSymbolTable *symbols) {
    if (!X || !method) return xr_null();
    
    /* 准备参数：[this, arg1, arg2, ...] */
    XrValue *full_args = (XrValue*)xmem_alloc(sizeof(XrValue) * (argc + 1));
    full_args[0] = this_val;  /* 第一个参数是 this */
    for (int i = 0; i < argc; i++) {
        full_args[i + 1] = args[i];
    }
    
    /* 调用底层函数 */
    /* 方法调用：当前使用简化实现，字节码VM阶段完善 */
    fprintf(stderr, "方法调用: 暂未实现（等待字节码VM支持）\n");
    XrValue result = xr_null();
    
    xmem_free(full_args);
    return result;
}

/*
** 调用静态方法（无this绑定）
*/
XrValue xr_method_call_static(XrayState *X, XrMethod *method, 
                               XrValue *args, int argc,
                               XSymbolTable *symbols) {
    if (!X || !method) return xr_null();
    
    assert(method->is_static && "Method must be static");
    
    /* 直接调用底层函数，无this绑定 */
    /* 方法调用：当前使用简化实现，字节码VM阶段完善 */
    (void)args; (void)argc; (void)symbols;
    fprintf(stderr, "静态方法调用: 暂未实现（等待字节码VM支持）\n");
    return xr_null();
}

/* ========== 辅助函数实现 ========== */

/*
** 打印方法信息（调试用）
*/
void xr_method_print(XrMethod *method) {
    if (!method) {
        printf("null method\n");
        return;
    }
    
    /* 打印修饰符 */
    if (method->is_static) printf("static ");
    if (method->is_private) printf("private ");
    if (method->is_getter) printf("get ");
    if (method->is_setter) printf("set ");
    
    /* 打印方法名 */
    printf("%s(", method->name);
    
    /* 打印参数（从函数对象获取）*/
    if (method->func) {
        for (int i = 0; i < method->func->param_count; i++) {
            if (i > 0) printf(", ");
            printf("%s", method->func->parameters[i]);
            if (method->func->param_types && method->func->param_types[i]) {
                printf(": %s", xr_type_kind_name(method->func->param_types[i]->kind));
            }
        }
    }
    
    printf(")");
    
    /* 打印返回类型 */
    if (method->func && method->func->return_type) {
        printf(": %s", xr_type_kind_name(method->func->return_type->kind));
    }
    
    printf("\n");
}

/*
** 获取方法签名字符串
*/
char* xr_method_signature(XrMethod *method) {
    if (!method) return str_dup("null method");
    
    /* 预估长度（简化实现）*/
    size_t len = 256;
    char *sig = (char*)xmem_alloc(len);
    char *p = sig;
    
    /* 添加修饰符 */
    if (method->is_static) {
        p += sprintf(p, "static ");
    }
    if (method->is_private) {
        p += sprintf(p, "private ");
    }
    if (method->is_getter) {
        p += sprintf(p, "get ");
    }
    if (method->is_setter) {
        p += sprintf(p, "set ");
    }
    
    /* 添加方法名和参数 */
    p += sprintf(p, "%s(", method->name);
    
    if (method->func) {
        for (int i = 0; i < method->func->param_count; i++) {
            if (i > 0) p += sprintf(p, ", ");
            p += sprintf(p, "%s", method->func->parameters[i]);
            if (method->func->param_types && method->func->param_types[i]) {
                p += sprintf(p, ": %s", 
                            xr_type_kind_name(method->func->param_types[i]->kind));
            }
        }
    }
    
    p += sprintf(p, ")");
    
    /* 添加返回类型 */
    if (method->func && method->func->return_type) {
        p += sprintf(p, ": %s", 
                    xr_type_kind_name(method->func->return_type->kind));
    }
    
    return sig;
}


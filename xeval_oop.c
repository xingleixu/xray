/*
** xeval_oop.c
** Xray OOP求值实现
** 
** v0.12.0：第十二阶段 - Evaluator扩展
** 
** 功能：
**   - 求值class声明
**   - 求值new表达式
**   - 求值this/super表达式
**   - 求值成员赋值
*/

#include "xeval.h"
#include "xast.h"
#include "xclass.h"
#include "xinstance.h"
#include "xmethod.h"
#include "xtype.h"
#include "xscope.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ========== 类声明求值 ========== */

/*
** 求值类声明
** 
** 流程：
** 1. 创建XrClass对象
** 2. 解析字段定义并添加到类
** 3. 解析方法定义并添加到类
** 4. 注册类到当前作用域
*/
XrValue xr_eval_class_decl(XrayState *X, AstNode *node, XSymbolTable *symbols) {
    if (!node || node->type != AST_CLASS_DECL) {
        return xr_null();
    }
    
    ClassDeclNode *class_decl = &node->as.class_decl;
    
    /* 1. 查找超类（如果有）*/
    XrClass *super = NULL;
    if (class_decl->super_name) {
        XrValue super_val;
        if (!xsymboltable_get(symbols, class_decl->super_name, &super_val) ||
            !xr_is_class(super_val)) {
            fprintf(stderr, "运行时错误: 超类'%s'不存在或不是类\n",
                    class_decl->super_name);
            return xr_null();
        }
        super = xr_to_class(super_val);
    }
    
    /* 2. 创建类对象 */
    XrClass *cls = xr_class_new(X, class_decl->name, super);
    
    /* 3. 添加字段定义 */
    for (int i = 0; i < class_decl->field_count; i++) {
        AstNode *field_node = class_decl->fields[i];
        if (field_node->type != AST_FIELD_DECL) continue;
        
        FieldDeclNode *field = &field_node->as.field_decl;
        
        /* 解析类型 */
        XrTypeInfo *field_type = NULL;
        if (field->type_name) {
            /* 简化：直接根据类型名获取内置类型 */
            if (strcmp(field->type_name, "int") == 0) {
                field_type = xr_type_int(X);
            } else if (strcmp(field->type_name, "float") == 0) {
                field_type = xr_type_float(X);
            } else if (strcmp(field->type_name, "string") == 0) {
                field_type = xr_type_string(X);
            } else if (strcmp(field->type_name, "bool") == 0) {
                field_type = xr_type_bool(X);
            } else {
                field_type = xr_type_any(X);  /* 未知类型 */
            }
        } else {
            field_type = xr_type_any(X);
        }
        
        /* 添加字段到类 */
        xr_class_add_field(cls, field->name, field_type);
        
        /* v0.12.1: 处理访问控制 */
        if (field->is_private) {
            xr_class_mark_field_private(cls, field->name);
        }
        
        /* v0.12.2: 处理静态字段 */
        if (field->is_static) {
            /* 静态字段：求值初始值并存储在类中 */
            XrValue initial_val = xr_null();
            if (field->initializer) {
                LoopControl loop = {LOOP_NONE, 0};
                ReturnControl ret = {0};
                ret.return_value = xr_null();
                initial_val = xr_eval_internal(X, field->initializer, symbols, &loop, &ret);
            }
            xr_class_add_static_field(cls, field->name, initial_val);
        }
    }
    
    /* 4. 添加方法定义 */
    for (int i = 0; i < class_decl->method_count; i++) {
        AstNode *method_node = class_decl->methods[i];
        if (method_node->type != AST_METHOD_DECL) continue;
        
        MethodDeclNode *method = &method_node->as.method_decl;
        
        /* 解析参数类型 */
        XrTypeInfo **param_types = NULL;
        if (method->param_types) {
            param_types = (XrTypeInfo**)malloc(sizeof(XrTypeInfo*) * method->param_count);
            for (int j = 0; j < method->param_count; j++) {
                if (method->param_types[j]) {
                    /* 简化：根据类型名获取内置类型 */
                    if (strcmp(method->param_types[j], "int") == 0) {
                        param_types[j] = xr_type_int(X);
                    } else if (strcmp(method->param_types[j], "float") == 0) {
                        param_types[j] = xr_type_float(X);
                    } else if (strcmp(method->param_types[j], "string") == 0) {
                        param_types[j] = xr_type_string(X);
                    } else {
                        param_types[j] = xr_type_any(X);
                    }
                } else {
                    param_types[j] = xr_type_any(X);
                }
            }
        }
        
        /* 解析返回类型 */
        XrTypeInfo *return_type = NULL;
        if (method->return_type) {
            if (strcmp(method->return_type, "int") == 0) {
                return_type = xr_type_int(X);
            } else if (strcmp(method->return_type, "float") == 0) {
                return_type = xr_type_float(X);
            } else if (strcmp(method->return_type, "string") == 0) {
                return_type = xr_type_string(X);
            } else if (strcmp(method->return_type, "void") == 0) {
                return_type = xr_type_void(X);
            } else {
                return_type = xr_type_any(X);
            }
        } else {
            return_type = xr_type_void(X);
        }
        
        /* 创建函数对象 */
        XrFunction *func = xr_function_new(method->name,
                                           method->parameters,
                                           param_types,
                                           method->param_count,
                                           return_type,
                                           method->body);
        
        /* 创建方法对象 */
        XrMethod *xr_method = xr_method_new(X, method->name, func, method->is_static);
        
        /* 设置方法属性 */
        if (method->is_constructor) {
            xr_method_mark_constructor(xr_method);
        }
        
        /* v0.12.1: 设置访问控制 */
        if (method->is_private) {
            xr_method_mark_private(xr_method);
        }
        
        /* v0.12.3: 设置Getter/Setter（待实施）*/
        if (method->is_getter) {
            xr_method_mark_getter(xr_method);
        }
        if (method->is_setter) {
            xr_method_mark_setter(xr_method);
        }
        
        /* v0.12.2: 添加方法到类（根据is_static标志）*/
        if (method->is_static) {
            /* 静态方法 */
            xr_class_add_static_method(cls, xr_method);
        } else {
            /* 实例方法 */
            xr_class_add_method(cls, xr_method);
        }
    }
    
    /* 5. 将类注册到符号表 */
    XrValue class_value = xr_value_from_class(cls);
    xsymboltable_define(symbols, class_decl->name, class_value, false);
    
    return class_value;
}

/* ========== new表达式求值 ========== */

/*
** 求值new表达式
** 
** 流程：
** 1. 从符号表查找类
** 2. 求值构造参数
** 3. 调用xr_instance_construct创建实例
*/
XrValue xr_eval_new_expr(XrayState *X, AstNode *node, XSymbolTable *symbols) {
    if (!node || node->type != AST_NEW_EXPR) {
        return xr_null();
    }
    
    NewExprNode *new_expr = &node->as.new_expr;
    
    /* 1. 查找类 */
    XrValue class_val;
    if (!xsymboltable_get(symbols, new_expr->class_name, &class_val) ||
        !xr_is_class(class_val)) {
        fprintf(stderr, "运行时错误: 类'%s'不存在\n", new_expr->class_name);
        return xr_null();
    }
    
    XrClass *cls = xr_to_class(class_val);
    
    /* 2. 求值构造参数 */
    XrValue *args = NULL;
    if (new_expr->arg_count > 0) {
        args = (XrValue*)malloc(sizeof(XrValue) * new_expr->arg_count);
        for (int i = 0; i < new_expr->arg_count; i++) {
            /* TODO: 需要添加loop和ret参数 */
            /* 暂时简化：直接递归求值（假设有全局eval函数）*/
            args[i] = xr_eval(X, new_expr->arguments[i]);
        }
    }
    
    /* 3. 创建实例（调用构造函数）*/
    XrValue instance = xr_instance_construct(X, cls, args, new_expr->arg_count, symbols);
    
    if (args) free(args);
    
    return instance;
}

/* ========== this表达式求值 ========== */

/*
** 求值this表达式
** 
** 流程：
** 1. 从符号表查找"this"变量
** 2. 返回实例值
*/
XrValue xr_eval_this_expr(XrayState *X, AstNode *node, XSymbolTable *symbols) {
    if (!node || node->type != AST_THIS_EXPR) {
        return xr_null();
    }
    
    /* 从符号表查找this */
    XrValue this_val;
    if (!xsymboltable_get(symbols, "this", &this_val) ||
        !xr_is_instance(this_val)) {
        fprintf(stderr, "运行时错误: 'this'只能在方法中使用\n");
        return xr_null();
    }
    
    return this_val;
}

/* ========== super调用求值 ========== */

/*
** 求值super调用
** 
** 流程：
** 1. 获取当前实例（this）
** 2. 获取超类
** 3. 调用超类方法
*/
XrValue xr_eval_super_call(XrayState *X, AstNode *node, XSymbolTable *symbols) {
    if (!node || node->type != AST_SUPER_CALL) {
        return xr_null();
    }
    
    SuperCallNode *super_call = &node->as.super_call;
    
    /* 1. 获取当前实例 */
    XrValue this_val;
    if (!xsymboltable_get(symbols, "this", &this_val) ||
        !xr_is_instance(this_val)) {
        fprintf(stderr, "运行时错误: 'super'只能在方法中使用\n");
        return xr_null();
    }
    
    XrInstance *inst = xr_to_instance(this_val);
    
    /* 2. 获取超类 */
    if (!inst->klass->super) {
        fprintf(stderr, "运行时错误: 类'%s'没有超类\n", inst->klass->name);
        return xr_null();
    }
    
    XrClass *super_class = inst->klass->super;
    
    /* 3. 查找超类方法 */
    const char *method_name = super_call->method_name ? 
                              super_call->method_name : "constructor";
    
    XrMethod *method = (XrMethod*)xr_hashmap_get(super_class->methods, method_name);
    if (!method) {
        fprintf(stderr, "运行时错误: 超类方法'%s'不存在\n", method_name);
        return xr_null();
    }
    
    /* 4. 求值参数 */
    XrValue *args = NULL;
    if (super_call->arg_count > 0) {
        args = (XrValue*)malloc(sizeof(XrValue) * super_call->arg_count);
        for (int i = 0; i < super_call->arg_count; i++) {
            args[i] = xr_eval(X, super_call->arguments[i]);
        }
    }
    
    /* 5. 调用超类方法 */
    XrValue result = xr_method_call(X, method, this_val, args, 
                                    super_call->arg_count, symbols);
    
    if (args) free(args);
    
    return result;
}

/* ========== 成员赋值求值 ========== */

/*
** 求值成员赋值
** 
** 流程：
** 1. 求值对象表达式
** 2. 求值赋值表达式
** 3. 调用xr_instance_set_field
*/
XrValue xr_eval_member_set(XrayState *X, AstNode *node, XSymbolTable *symbols,
                           LoopControl *loop, ReturnControl *ret) {
    if (!node || node->type != AST_MEMBER_SET) {
        return xr_null();
    }
    
    MemberSetNode *member_set = &node->as.member_set;
    
    /* 1. 求值对象表达式（使用传入的symbols） */
    XrValue obj_val = xr_eval_internal(X, member_set->object, symbols, loop, ret);
    if (!xr_is_instance(obj_val)) {
        fprintf(stderr, "运行时错误: 只能设置实例的成员\n");
        return xr_null();
    }
    
    XrInstance *inst = xr_to_instance(obj_val);
    
    /* 2. 求值赋值表达式（使用传入的symbols） */
    XrValue value = xr_eval_internal(X, member_set->value, symbols, loop, ret);
    
    /* 3. 设置字段 */
    xr_instance_set_field(inst, member_set->member, value);
    
    return value;
}


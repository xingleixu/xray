/*
** xscope.c
** Xray 作用域和符号表实现
*/

#include "xscope.h"
#include <stdlib.h>
#include <string.h>

/* 初始变量容量 */
#define INIT_VAR_CAPACITY 8

/*
** ============================================================================
** 作用域操作函数
** ============================================================================
*/

/*
** 创建新作用域
*/
XScope *xscope_new(int depth, XScope *enclosing) {
    XScope *scope = (XScope *)malloc(sizeof(XScope));
    if (!scope) return NULL;
    
    scope->enclosing = enclosing;
    scope->depth = depth;
    scope->var_count = 0;
    scope->var_capacity = INIT_VAR_CAPACITY;
    scope->variables = (XVariable *)malloc(sizeof(XVariable) * INIT_VAR_CAPACITY);
    
    if (!scope->variables) {
        free(scope);
        return NULL;
    }
    
    return scope;
}

/*
** 释放作用域
** 释放作用域中的所有变量名和作用域本身
*/
void xscope_free(XScope *scope) {
    if (!scope) return;
    
    /* 释放所有变量名 */
    for (int i = 0; i < scope->var_count; i++) {
        free(scope->variables[i].name);
    }
    
    free(scope->variables);
    free(scope);
}

/*
** 在作用域中添加变量
** 如果变量已存在于当前作用域，返回 false
*/
bool xscope_add_variable(XScope *scope, const char *name, XrValue value, bool is_const) {
    /* 检查变量是否已存在于当前作用域 */
    for (int i = 0; i < scope->var_count; i++) {
        if (strcmp(scope->variables[i].name, name) == 0) {
            return false;  /* 变量已存在 */
        }
    }
    
    /* 扩容检查 */
    if (scope->var_count >= scope->var_capacity) {
        int new_capacity = scope->var_capacity * 2;
        XVariable *new_vars = (XVariable *)realloc(scope->variables, 
                                                   sizeof(XVariable) * new_capacity);
        if (!new_vars) return false;
        
        scope->variables = new_vars;
        scope->var_capacity = new_capacity;
    }
    
    /* 添加新变量 */
    XVariable *var = &scope->variables[scope->var_count++];
    var->name = strdup(name);  /* 复制变量名 */
    if (!var->name) {
        scope->var_count--;
        return false;
    }
    
    var->value = value;
    var->is_const = is_const;
    var->depth = scope->depth;
    
    return true;
}

/*
** 在作用域中查找变量（只查找当前作用域）
*/
XVariable *xscope_find_variable(XScope *scope, const char *name) {
    for (int i = 0; i < scope->var_count; i++) {
        if (strcmp(scope->variables[i].name, name) == 0) {
            return &scope->variables[i];
        }
    }
    return NULL;
}

/*
** ============================================================================
** 符号表操作函数
** ============================================================================
*/

/*
** 创建新的符号表
*/
XSymbolTable *xsymboltable_new(void) {
    XSymbolTable *table = (XSymbolTable *)malloc(sizeof(XSymbolTable));
    if (!table) return NULL;
    
    /* 创建全局作用域 */
    table->global = xscope_new(0, NULL);
    if (!table->global) {
        free(table);
        return NULL;
    }
    
    table->current = table->global;
    table->scope_depth = 0;
    
    return table;
}

/*
** 释放符号表
** 释放所有作用域（从当前作用域到全局作用域）
*/
void xsymboltable_free(XSymbolTable *table) {
    if (!table) return;
    
    /* 释放所有作用域 */
    XScope *scope = table->current;
    while (scope) {
        XScope *enclosing = scope->enclosing;
        xscope_free(scope);
        scope = enclosing;
    }
    
    free(table);
}

/*
** 进入新作用域
*/
void xsymboltable_begin_scope(XSymbolTable *table) {
    int new_depth = table->scope_depth + 1;
    XScope *new_scope = xscope_new(new_depth, table->current);
    
    if (new_scope) {
        table->current = new_scope;
        table->scope_depth = new_depth;
    }
}

/*
** 退出当前作用域
** 不能退出全局作用域
*/
void xsymboltable_end_scope(XSymbolTable *table) {
    if (table->current == table->global) {
        return;  /* 不能退出全局作用域 */
    }
    
    XScope *old_scope = table->current;
    table->current = old_scope->enclosing;
    table->scope_depth--;
    
    xscope_free(old_scope);
}

/*
** 定义变量
** 在当前作用域中定义一个新变量
*/
bool xsymboltable_define(XSymbolTable *table, const char *name, 
                         XrValue value, bool is_const) {
    return xscope_add_variable(table->current, name, value, is_const);
}

/*
** 查找变量
** 从当前作用域开始向外层查找变量（作用域链查找）
*/
XVariable *xsymboltable_resolve(XSymbolTable *table, const char *name) {
    XScope *scope = table->current;
    
    /* 沿着作用域链向外查找 */
    while (scope) {
        XVariable *var = xscope_find_variable(scope, name);
        if (var) {
            return var;  /* 找到变量 */
        }
        scope = scope->enclosing;
    }
    
    return NULL;  /* 变量未定义 */
}

/*
** 赋值变量
** 修改已存在的变量的值
*/
bool xsymboltable_assign(XSymbolTable *table, const char *name, XrValue value) {
    XVariable *var = xsymboltable_resolve(table, name);
    
    if (!var) {
        return false;  /* 变量未定义 */
    }
    
    if (var->is_const) {
        return false;  /* 不能修改常量 */
    }
    
    var->value = value;
    return true;
}

/*
** 获取变量值
** 查找变量并获取其值
*/
bool xsymboltable_get(XSymbolTable *table, const char *name, XrValue *out_value) {
    XVariable *var = xsymboltable_resolve(table, name);
    
    if (!var) {
        return false;  /* 变量未定义 */
    }
    
    *out_value = var->value;
    return true;
}


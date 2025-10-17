/*
** xscope.h
** Xray 作用域和符号表管理
** 负责变量的定义、查找和作用域管理
*/

#ifndef xscope_h
#define xscope_h

#include "xvalue.h"
#include <stdbool.h>

/*
** 变量信息
** 存储变量的名称、值、是否为常量等信息
*/
typedef struct {
    char *name;          /* 变量名（需要复制字符串） */
    XrValue value;       /* 变量值 */
    bool is_const;       /* 是否为常量 */
    int depth;           /* 作用域深度（0=全局，1+=局部） */
} XVariable;

/*
** 作用域结构
** 表示一个作用域，包含该作用域中定义的所有变量
*/
typedef struct XScope {
    struct XScope *enclosing;  /* 外层作用域（形成作用域链） */
    XVariable *variables;      /* 变量数组 */
    int var_count;            /* 当前变量数量 */
    int var_capacity;         /* 变量数组容量 */
    int depth;                /* 作用域深度 */
} XScope;

/*
** 符号表（作用域管理器）
** 管理所有作用域，提供变量的定义、查找、赋值等操作
*/
typedef struct {
    XScope *current;          /* 当前作用域 */
    XScope *global;           /* 全局作用域 */
    int scope_depth;          /* 当前作用域深度 */
} XSymbolTable;

/*
** 符号表操作函数
*/

/* 创建新的符号表 */
XSymbolTable *xsymboltable_new(void);

/* 释放符号表 */
void xsymboltable_free(XSymbolTable *table);

/* 进入新作用域 */
void xsymboltable_begin_scope(XSymbolTable *table);

/* 退出当前作用域 */
void xsymboltable_end_scope(XSymbolTable *table);

/* 
** 定义变量
** 在当前作用域中定义一个新变量
** 如果变量已存在，返回 false
*/
bool xsymboltable_define(XSymbolTable *table, const char *name, 
                         XrValue value, bool is_const);

/* 
** 查找变量
** 从当前作用域开始向外层查找变量
** 找不到返回 NULL
*/
XVariable *xsymboltable_resolve(XSymbolTable *table, const char *name);

/* 
** 赋值变量
** 修改已存在的变量的值
** 不能修改常量，如果变量不存在或是常量，返回 false
*/
bool xsymboltable_assign(XSymbolTable *table, const char *name, XrValue value);

/* 
** 获取变量值
** 查找变量并获取其值
** 找不到返回 false
*/
bool xsymboltable_get(XSymbolTable *table, const char *name, XrValue *out_value);

/*
** 作用域操作函数（内部使用）
*/

/* 创建新作用域 */
XScope *xscope_new(int depth, XScope *enclosing);

/* 释放作用域 */
void xscope_free(XScope *scope);

/* 在作用域中添加变量 */
bool xscope_add_variable(XScope *scope, const char *name, XrValue value, bool is_const);

/* 在作用域中查找变量（只查找当前作用域） */
XVariable *xscope_find_variable(XScope *scope, const char *name);

#endif /* xscope_h */


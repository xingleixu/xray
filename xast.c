/*
** xast.c
** Xray AST 实现
** 实现抽象语法树节点的创建、操作和释放
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "xast.h"
#include "xtype.h"  /* 新增：用于访问xr_builtin_*_type */

/* 程序节点初始容量 */
#define INITIAL_CAPACITY 8

/*
** 分配 AST 节点内存
** 返回新分配的节点指针
*/
static AstNode *alloc_node(XrayState *X, AstNodeType type, int line) {
    AstNode *node = (AstNode *)malloc(sizeof(AstNode));
    if (node == NULL) {
        fprintf(stderr, "内存分配失败\n");
        exit(1);
    }
    node->type = type;
    node->line = line;
    return node;
}

/* ========== 字面量节点创建 ========== */

/*
** 创建整数字面量节点
*/
AstNode *xr_ast_literal_int(XrayState *X, xr_Integer value, int line) {
    AstNode *node = alloc_node(X, AST_LITERAL_INT, line);
    node->as.literal.value = xr_int(value);  /* 新API */
    return node;
}

/*
** 创建浮点数字面量节点
*/
AstNode *xr_ast_literal_float(XrayState *X, xr_Number value, int line) {
    AstNode *node = alloc_node(X, AST_LITERAL_FLOAT, line);
    node->as.literal.value = xr_float(value);  /* 新API */
    return node;
}

/*
** 创建字符串字面量节点
** v0.10.0: 使用字符串驻留系统
*/
AstNode *xr_ast_literal_string(XrayState *X, const char *value, int line) {
    /* 引用字符串驻留函数 */
    extern XrString* xr_string_intern(const char *chars, size_t length, uint32_t hash);
    extern XrValue xr_string_value(XrString *str);
    
    (void)X;  /* 暂时未使用 */
    
    AstNode *node = alloc_node(X, AST_LITERAL_STRING, line);
    
    /* 使用字符串驻留（v0.10.0新增）*/
    XrString *str = xr_string_intern(value, strlen(value), 0);
    node->as.literal.value = xr_string_value(str);
    
    return node;
}

/*
** 创建 null 字面量节点
*/
AstNode *xr_ast_literal_null(XrayState *X, int line) {
    AstNode *node = alloc_node(X, AST_LITERAL_NULL, line);
    node->as.literal.value = xr_null();  /* 新API */
    return node;
}

/*
** 创建模板字符串节点（v0.10.0 Day 5新增）
** 参数：
**   parts: 字符串片段和表达式数组（交替）
**   part_count: 片段数量
*/
AstNode *xr_ast_template_string(XrayState *X, AstNode **parts, int part_count, int line) {
    (void)X;  /* 暂时未使用 */
    
    AstNode *node = alloc_node(X, AST_TEMPLATE_STRING, line);
    
    /* 分配并复制parts数组 */
    node->as.template_str.parts = (AstNode**)malloc(sizeof(AstNode*) * part_count);
    for (int i = 0; i < part_count; i++) {
        node->as.template_str.parts[i] = parts[i];
    }
    node->as.template_str.part_count = part_count;
    
    return node;
}

/*
** 创建布尔字面量节点
** value: 0 表示 false，非 0 表示 true
*/
AstNode *xr_ast_literal_bool(XrayState *X, int value, int line) {
    AstNode *node = alloc_node(X, value ? AST_LITERAL_TRUE : AST_LITERAL_FALSE, line);
    node->as.literal.value = xr_bool(value ? 1 : 0);  /* 新API */
    return node;
}

/* ========== 运算符节点创建 ========== */

/*
** 创建二元运算节点
** type: 运算符类型（加减乘除、比较、逻辑等）
** left: 左操作数
** right: 右操作数
*/
AstNode *xr_ast_binary(XrayState *X, AstNodeType type,
                       AstNode *left, AstNode *right, int line) {
    AstNode *node = alloc_node(X, type, line);
    node->as.binary.left = left;
    node->as.binary.right = right;
    return node;
}

/*
** 创建一元运算节点
** type: 运算符类型（取负、逻辑非）
** operand: 操作数
*/
AstNode *xr_ast_unary(XrayState *X, AstNodeType type,
                      AstNode *operand, int line) {
    AstNode *node = alloc_node(X, type, line);
    node->as.unary.operand = operand;
    return node;
}

/* ========== 其他节点创建 ========== */

/*
** 创建分组节点（括号表达式）
*/
AstNode *xr_ast_grouping(XrayState *X, AstNode *expr, int line) {
    AstNode *node = alloc_node(X, AST_GROUPING, line);
    node->as.grouping = expr;
    return node;
}

/*
** 创建表达式语句节点
*/
AstNode *xr_ast_expr_stmt(XrayState *X, AstNode *expr, int line) {
    AstNode *node = alloc_node(X, AST_EXPR_STMT, line);
    node->as.expr_stmt = expr;
    return node;
}

/*
** 创建 print 语句节点
*/
AstNode *xr_ast_print_stmt(XrayState *X, AstNode *expr, int line) {
    AstNode *node = alloc_node(X, AST_PRINT_STMT, line);
    node->as.print_stmt.expr = expr;
    return node;
}

/* ========== 程序节点操作 ========== */

/*
** 创建程序节点
** 程序节点包含多个语句
*/
AstNode *xr_ast_program(XrayState *X) {
    AstNode *node = alloc_node(X, AST_PROGRAM, 0);
    node->as.program.statements = NULL;
    node->as.program.count = 0;
    node->as.program.capacity = 0;
    return node;
}

/*
** 向程序节点添加语句
** 使用动态数组存储语句列表
*/
void xr_ast_program_add(XrayState *X, AstNode *program, AstNode *stmt) {
    /* 确保有足够空间 */
    if (program->as.program.count >= program->as.program.capacity) {
        int old_capacity = program->as.program.capacity;
        int new_capacity = old_capacity < INITIAL_CAPACITY ? 
                          INITIAL_CAPACITY : old_capacity * 2;
        
        program->as.program.statements = (AstNode **)realloc(
            program->as.program.statements,
            sizeof(AstNode *) * new_capacity
        );
        
        if (program->as.program.statements == NULL) {
            fprintf(stderr, "内存分配失败\n");
            exit(1);
        }
        
        program->as.program.capacity = new_capacity;
    }
    
    /* 添加语句 */
    program->as.program.statements[program->as.program.count++] = stmt;
}

/* ========== 代码块节点操作 ========== */

/*
** 创建代码块节点
** 代码块包含多个语句
*/
AstNode *xr_ast_block(XrayState *X, int line) {
    AstNode *node = alloc_node(X, AST_BLOCK, line);
    node->as.block.statements = NULL;
    node->as.block.count = 0;
    node->as.block.capacity = 0;
    return node;
}

/*
** 向代码块添加语句
*/
void xr_ast_block_add(XrayState *X, AstNode *block, AstNode *stmt) {
    /* 确保有足够空间 */
    if (block->as.block.count >= block->as.block.capacity) {
        int old_capacity = block->as.block.capacity;
        int new_capacity = old_capacity < INITIAL_CAPACITY ? 
                          INITIAL_CAPACITY : old_capacity * 2;
        
        block->as.block.statements = (AstNode **)realloc(
            block->as.block.statements,
            sizeof(AstNode *) * new_capacity
        );
        
        if (block->as.block.statements == NULL) {
            fprintf(stderr, "内存分配失败\n");
            exit(1);
        }
        
        block->as.block.capacity = new_capacity;
    }
    
    /* 添加语句 */
    block->as.block.statements[block->as.block.count++] = stmt;
}

/* ========== 变量相关节点创建 ========== */

/*
** 创建变量声明节点
** name: 变量名
** initializer: 初始化表达式（可以为 NULL）
** is_const: 是否为常量
*/
AstNode *xr_ast_var_decl(XrayState *X, const char *name, 
                         AstNode *initializer, bool is_const, int line) {
    AstNode *node = alloc_node(X, is_const ? AST_CONST_DECL : AST_VAR_DECL, line);
    node->as.var_decl.name = strdup(name);
    node->as.var_decl.initializer = initializer;
    node->as.var_decl.is_const = is_const;
    
    if (node->as.var_decl.name == NULL) {
        fprintf(stderr, "内存分配失败\n");
        exit(1);
    }
    
    return node;
}

/*
** 创建变量引用节点
** name: 变量名
*/
AstNode *xr_ast_variable(XrayState *X, const char *name, int line) {
    AstNode *node = alloc_node(X, AST_VARIABLE, line);
    node->as.variable.name = strdup(name);
    
    if (node->as.variable.name == NULL) {
        fprintf(stderr, "内存分配失败\n");
        exit(1);
    }
    
    return node;
}

/*
** 创建赋值节点
** name: 变量名
** value: 赋值表达式
*/
AstNode *xr_ast_assignment(XrayState *X, const char *name, 
                           AstNode *value, int line) {
    AstNode *node = alloc_node(X, AST_ASSIGNMENT, line);
    node->as.assignment.name = strdup(name);
    node->as.assignment.value = value;
    
    if (node->as.assignment.name == NULL) {
        fprintf(stderr, "内存分配失败\n");
        exit(1);
    }
    
    return node;
}

/* ========== 控制流节点创建 ========== */

/*
** 创建 if 语句节点
** condition: 条件表达式
** then_branch: then 分支（必须是 block）
** else_branch: else 分支（可选，可以是 block 或 if）
*/
AstNode *xr_ast_if_stmt(XrayState *X, AstNode *condition, 
                        AstNode *then_branch, AstNode *else_branch, int line) {
    AstNode *node = alloc_node(X, AST_IF_STMT, line);
    node->as.if_stmt.condition = condition;
    node->as.if_stmt.then_branch = then_branch;
    node->as.if_stmt.else_branch = else_branch;
    return node;
}

/*
** 创建 while 循环节点
** condition: 循环条件
** body: 循环体（必须是 block）
*/
AstNode *xr_ast_while_stmt(XrayState *X, AstNode *condition, 
                           AstNode *body, int line) {
    AstNode *node = alloc_node(X, AST_WHILE_STMT, line);
    node->as.while_stmt.condition = condition;
    node->as.while_stmt.body = body;
    return node;
}

/*
** 创建 for 循环节点
** initializer: 初始化（可选）
** condition: 条件（可选）
** increment: 更新（可选）
** body: 循环体（必须是 block）
*/
AstNode *xr_ast_for_stmt(XrayState *X, AstNode *initializer, 
                         AstNode *condition, AstNode *increment, 
                         AstNode *body, int line) {
    AstNode *node = alloc_node(X, AST_FOR_STMT, line);
    node->as.for_stmt.initializer = initializer;
    node->as.for_stmt.condition = condition;
    node->as.for_stmt.increment = increment;
    node->as.for_stmt.body = body;
    return node;
}

/*
** 创建 break 语句节点
*/
AstNode *xr_ast_break_stmt(XrayState *X, int line) {
    AstNode *node = alloc_node(X, AST_BREAK_STMT, line);
    node->as.break_stmt.placeholder = 0;
    return node;
}

/*
** 创建 continue 语句节点
*/
AstNode *xr_ast_continue_stmt(XrayState *X, int line) {
    AstNode *node = alloc_node(X, AST_CONTINUE_STMT, line);
    node->as.continue_stmt.placeholder = 0;
    return node;
}

/* ========== 函数相关节点创建 ========== */

/*
** 创建函数声明节点
** name: 函数名
** parameters: 参数列表（字符串数组）
** param_count: 参数数量
** body: 函数体（必须是 block 节点）
*/
AstNode *xr_ast_function_decl(XrayState *X, const char *name,
                              char **parameters, int param_count,
                              AstNode *body, int line) {
    AstNode *node = alloc_node(X, AST_FUNCTION_DECL, line);
    
    /* 复制函数名 */
    node->as.function_decl.name = (char *)malloc(strlen(name) + 1);
    strcpy(node->as.function_decl.name, name);
    
    /* 复制参数列表 */
    node->as.function_decl.param_count = param_count;
    if (param_count > 0) {
        node->as.function_decl.parameters = (char **)malloc(sizeof(char *) * param_count);
        for (int i = 0; i < param_count; i++) {
            node->as.function_decl.parameters[i] = (char *)malloc(strlen(parameters[i]) + 1);
            strcpy(node->as.function_decl.parameters[i], parameters[i]);
        }
    } else {
        node->as.function_decl.parameters = NULL;
    }
    
    node->as.function_decl.body = body;
    return node;
}

/*
** 创建函数表达式节点（箭头函数/匿名函数）
** parameters: 参数列表（字符串数组）
** param_count: 参数数量
** body: 函数体（必须是 block）
*/
AstNode *xr_ast_function_expr(XrayState *X, char **parameters, int param_count,
                              AstNode *body, int line) {
    AstNode *node = alloc_node(X, AST_FUNCTION_EXPR, line);
    
    /* 匿名函数没有名称 */
    node->as.function_expr.name = NULL;
    
    /* 复制参数列表 */
    node->as.function_expr.param_count = param_count;
    if (param_count > 0) {
        node->as.function_expr.parameters = (char **)malloc(sizeof(char *) * param_count);
        for (int i = 0; i < param_count; i++) {
            node->as.function_expr.parameters[i] = (char *)malloc(strlen(parameters[i]) + 1);
            strcpy(node->as.function_expr.parameters[i], parameters[i]);
        }
    } else {
        node->as.function_expr.parameters = NULL;
    }
    
    node->as.function_expr.body = body;
    return node;
}

/*
** 创建函数调用节点
** callee: 被调用的表达式（通常是变量）
** arguments: 参数列表（表达式数组）
** arg_count: 参数数量
*/
AstNode *xr_ast_call_expr(XrayState *X, AstNode *callee,
                          AstNode **arguments, int arg_count, int line) {
    AstNode *node = alloc_node(X, AST_CALL_EXPR, line);
    node->as.call_expr.callee = callee;
    node->as.call_expr.arg_count = arg_count;
    
    /* 复制参数列表 */
    if (arg_count > 0) {
        node->as.call_expr.arguments = (AstNode **)malloc(sizeof(AstNode *) * arg_count);
        for (int i = 0; i < arg_count; i++) {
            node->as.call_expr.arguments[i] = arguments[i];
        }
    } else {
        node->as.call_expr.arguments = NULL;
    }
    
    return node;
}

/*
** 创建 return 语句节点
** value: 返回值表达式（可选，NULL 表示 return 无值）
*/
AstNode *xr_ast_return_stmt(XrayState *X, AstNode *value, int line) {
    AstNode *node = alloc_node(X, AST_RETURN_STMT, line);
    node->as.return_stmt.value = value;
    return node;
}

/* ========== 数组相关节点========== */

/*
** 创建数组字面量节点
** elements: 元素表达式数组
** count: 元素数量
*/
AstNode *xr_ast_array_literal(XrayState *X, AstNode **elements, int count, int line) {
    AstNode *node = alloc_node(X, AST_ARRAY_LITERAL, line);
    node->as.array_literal.count = count;
    
    /* 复制元素数组 */
    if (count > 0) {
        node->as.array_literal.elements = (AstNode **)malloc(sizeof(AstNode *) * count);
        for (int i = 0; i < count; i++) {
            node->as.array_literal.elements[i] = elements[i];
        }
    } else {
        node->as.array_literal.elements = NULL;
    }
    
    return node;
}

/*
** 创建Map字面量节点（v0.11.0）
** keys: 键表达式数组
** values: 值表达式数组
** count: 键值对数量
*/
AstNode *xr_ast_map_literal(XrayState *X, AstNode **keys, AstNode **values, int count, int line) {
    AstNode *node = alloc_node(X, AST_MAP_LITERAL, line);
    node->as.map_literal.count = count;
    
    /* 复制键数组 */
    if (count > 0) {
        node->as.map_literal.keys = (AstNode **)malloc(sizeof(AstNode *) * count);
        node->as.map_literal.values = (AstNode **)malloc(sizeof(AstNode *) * count);
        for (int i = 0; i < count; i++) {
            node->as.map_literal.keys[i] = keys[i];
            node->as.map_literal.values[i] = values[i];
        }
    } else {
        node->as.map_literal.keys = NULL;
        node->as.map_literal.values = NULL;
    }
    
    return node;
}

/*
** 创建索引访问节点
** array: 数组表达式
** index: 索引表达式
*/
AstNode *xr_ast_index_get(XrayState *X, AstNode *array, AstNode *index, int line) {
    AstNode *node = alloc_node(X, AST_INDEX_GET, line);
    node->as.index_get.array = array;
    node->as.index_get.index = index;
    return node;
}

/*
** 创建索引赋值节点
** array: 数组表达式
** index: 索引表达式
** value: 赋值表达式
*/
AstNode *xr_ast_index_set(XrayState *X, AstNode *array, AstNode *index, AstNode *value, int line) {
    AstNode *node = alloc_node(X, AST_INDEX_SET, line);
    node->as.index_set.array = array;
    node->as.index_set.index = index;
    node->as.index_set.value = value;
    return node;
}

/*
** 创建成员访问节点
** object: 对象表达式
** name: 成员名称
*/
AstNode *xr_ast_member_access(XrayState *X, AstNode *object, const char *name, int line) {
    AstNode *node = alloc_node(X, AST_MEMBER_ACCESS, line);
    node->as.member_access.object = object;
    node->as.member_access.name = (char *)malloc(strlen(name) + 1);
    strcpy(node->as.member_access.name, name);
    return node;
}

/* ========== AST 释放 ========== */

/* ========== OOP节点创建函数（v0.12.0）========== */

/*
** 创建类声明节点
*/
AstNode *xr_ast_class_decl(XrayState *X, const char *name, const char *super_name,
                           AstNode **fields, int field_count,
                           AstNode **methods, int method_count, int line) {
    AstNode *node = alloc_node(X, AST_CLASS_DECL, line);
    node->as.class_decl.name = (char*)name;
    node->as.class_decl.super_name = (char*)super_name;
    node->as.class_decl.fields = fields;
    node->as.class_decl.field_count = field_count;
    node->as.class_decl.methods = methods;
    node->as.class_decl.method_count = method_count;
    return node;
}

/*
** 创建字段声明节点
*/
AstNode *xr_ast_field_decl(XrayState *X, const char *name, const char *type_name,
                           bool is_private, bool is_static,
                           AstNode *initializer, int line) {
    AstNode *node = alloc_node(X, AST_FIELD_DECL, line);
    node->as.field_decl.name = (char*)name;
    node->as.field_decl.type_name = (char*)type_name;
    node->as.field_decl.is_private = is_private;
    node->as.field_decl.is_static = is_static;
    node->as.field_decl.initializer = initializer;
    return node;
}

/*
** 创建方法声明节点
*/
AstNode *xr_ast_method_decl(XrayState *X, const char *name,
                            char **parameters, char **param_types, int param_count,
                            const char *return_type, AstNode *body,
                            bool is_constructor, bool is_static, bool is_private,
                            bool is_getter, bool is_setter, int line) {
    AstNode *node = alloc_node(X, AST_METHOD_DECL, line);
    node->as.method_decl.name = (char*)name;
    node->as.method_decl.parameters = parameters;
    node->as.method_decl.param_types = param_types;
    node->as.method_decl.param_count = param_count;
    node->as.method_decl.return_type = (char*)return_type;
    node->as.method_decl.body = body;
    node->as.method_decl.is_constructor = is_constructor;
    node->as.method_decl.is_static = is_static;
    node->as.method_decl.is_private = is_private;
    node->as.method_decl.is_getter = is_getter;
    node->as.method_decl.is_setter = is_setter;
    return node;
}

/*
** 创建new表达式节点
*/
AstNode *xr_ast_new_expr(XrayState *X, const char *class_name,
                         AstNode **arguments, int arg_count, int line) {
    AstNode *node = alloc_node(X, AST_NEW_EXPR, line);
    node->as.new_expr.class_name = (char*)class_name;
    node->as.new_expr.arguments = arguments;
    node->as.new_expr.arg_count = arg_count;
    return node;
}

/*
** 创建this表达式节点
*/
AstNode *xr_ast_this_expr(XrayState *X, int line) {
    AstNode *node = alloc_node(X, AST_THIS_EXPR, line);
    node->as.this_expr.placeholder = 0;
    return node;
}

/*
** 创建super调用节点
*/
AstNode *xr_ast_super_call(XrayState *X, const char *method_name,
                           AstNode **arguments, int arg_count, int line) {
    AstNode *node = alloc_node(X, AST_SUPER_CALL, line);
    node->as.super_call.method_name = (char*)method_name;
    node->as.super_call.arguments = arguments;
    node->as.super_call.arg_count = arg_count;
    return node;
}

/*
** 创建成员赋值节点
*/
AstNode *xr_ast_member_set(XrayState *X, AstNode *object, const char *member,
                           AstNode *value, int line) {
    AstNode *node = alloc_node(X, AST_MEMBER_SET, line);
    node->as.member_set.object = object;
    node->as.member_set.member = (char*)member;
    node->as.member_set.value = value;
    return node;
}

/*
** 递归释放 AST 节点
** 释放节点及其所有子节点的内存
*/
void xr_ast_free(XrayState *X, AstNode *node) {
    if (node == NULL) return;
    
    switch (node->type) {
        /* 字面量节点 */
        case AST_LITERAL_INT:
        case AST_LITERAL_FLOAT:
        case AST_LITERAL_NULL:
        case AST_LITERAL_TRUE:
        case AST_LITERAL_FALSE:
            /* 简单值，直接释放 */
            break;
            
        case AST_LITERAL_STRING:
            /* v0.10.0: 字符串由驻留池管理，不在这里释放 */
            /* 驻留的字符串由xr_string_pool_free()统一释放 */
            break;
        
        /* 二元运算节点 */
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
            xr_ast_free(X, node->as.binary.left);
            xr_ast_free(X, node->as.binary.right);
            break;
        
        /* 一元运算节点 */
        case AST_UNARY_NEG:
        case AST_UNARY_NOT:
            xr_ast_free(X, node->as.unary.operand);
            break;
        
        /* 分组节点 */
        case AST_GROUPING:
            xr_ast_free(X, node->as.grouping);
            break;
        
        /* 表达式语句 */
        case AST_EXPR_STMT:
            xr_ast_free(X, node->as.expr_stmt);
            break;
        
        /* print 语句 */
        case AST_PRINT_STMT:
            xr_ast_free(X, node->as.print_stmt.expr);
            break;
        
        /* 代码块 */
        case AST_BLOCK:
            for (int i = 0; i < node->as.block.count; i++) {
                xr_ast_free(X, node->as.block.statements[i]);
            }
            if (node->as.block.statements != NULL) {
                free(node->as.block.statements);
            }
            break;
        
        /* 变量声明 */
        case AST_VAR_DECL:
        case AST_CONST_DECL:
            free(node->as.var_decl.name);
            if (node->as.var_decl.initializer != NULL) {
                xr_ast_free(X, node->as.var_decl.initializer);
            }
            break;
        
        /* 变量引用 */
        case AST_VARIABLE:
            free(node->as.variable.name);
            break;
        
        /* 赋值 */
        case AST_ASSIGNMENT:
            free(node->as.assignment.name);
            xr_ast_free(X, node->as.assignment.value);
            break;
        
        /* 控制流节点 */
        case AST_IF_STMT:
            xr_ast_free(X, node->as.if_stmt.condition);
            xr_ast_free(X, node->as.if_stmt.then_branch);
            xr_ast_free(X, node->as.if_stmt.else_branch);
            break;
        
        case AST_WHILE_STMT:
            xr_ast_free(X, node->as.while_stmt.condition);
            xr_ast_free(X, node->as.while_stmt.body);
            break;
        
        case AST_FOR_STMT:
            xr_ast_free(X, node->as.for_stmt.initializer);
            xr_ast_free(X, node->as.for_stmt.condition);
            xr_ast_free(X, node->as.for_stmt.increment);
            xr_ast_free(X, node->as.for_stmt.body);
            break;
        
        case AST_BREAK_STMT:
        case AST_CONTINUE_STMT:
            /* 无需释放额外数据 */
            break;
        
        /* 函数相关节点 */
        case AST_FUNCTION_DECL:
            /* 释放函数名 */
            if (node->as.function_decl.name != NULL) {
                free(node->as.function_decl.name);
            }
            /* 释放参数列表 */
            if (node->as.function_decl.parameters != NULL) {
                for (int i = 0; i < node->as.function_decl.param_count; i++) {
                    free(node->as.function_decl.parameters[i]);
                }
                free(node->as.function_decl.parameters);
            }
            /* 释放函数体 */
            if (node->as.function_decl.body != NULL) {
                xr_ast_free(X, node->as.function_decl.body);
            }
            break;
        
        case AST_CALL_EXPR:
            /* 释放被调用者 */
            if (node->as.call_expr.callee != NULL) {
                xr_ast_free(X, node->as.call_expr.callee);
            }
            /* 释放参数列表 */
            if (node->as.call_expr.arguments != NULL) {
                for (int i = 0; i < node->as.call_expr.arg_count; i++) {
                    xr_ast_free(X, node->as.call_expr.arguments[i]);
                }
                free(node->as.call_expr.arguments);
            }
            break;
        
        case AST_RETURN_STMT:
            /* 释放返回值表达式 */
            if (node->as.return_stmt.value != NULL) {
                xr_ast_free(X, node->as.return_stmt.value);
            }
            break;
        
        /* 数组相关节点 */
        case AST_ARRAY_LITERAL:
            /* 释放数组元素 */
            if (node->as.array_literal.elements != NULL) {
                for (int i = 0; i < node->as.array_literal.count; i++) {
                    xr_ast_free(X, node->as.array_literal.elements[i]);
                }
                free(node->as.array_literal.elements);
            }
            break;
        
        /* Map相关节点（v0.11.0）*/
        case AST_MAP_LITERAL:
            /* 释放键和值 */
            if (node->as.map_literal.keys != NULL) {
                for (int i = 0; i < node->as.map_literal.count; i++) {
                    xr_ast_free(X, node->as.map_literal.keys[i]);
                    xr_ast_free(X, node->as.map_literal.values[i]);
                }
                free(node->as.map_literal.keys);
                free(node->as.map_literal.values);
            }
            break;
        
        /* OOP相关节点（v0.12.0）*/
        case AST_CLASS_DECL:
            if (node->as.class_decl.name) free(node->as.class_decl.name);
            if (node->as.class_decl.super_name) free(node->as.class_decl.super_name);
            if (node->as.class_decl.fields) {
                for (int i = 0; i < node->as.class_decl.field_count; i++) {
                    xr_ast_free(X, node->as.class_decl.fields[i]);
                }
                free(node->as.class_decl.fields);
            }
            if (node->as.class_decl.methods) {
                for (int i = 0; i < node->as.class_decl.method_count; i++) {
                    xr_ast_free(X, node->as.class_decl.methods[i]);
                }
                free(node->as.class_decl.methods);
            }
            break;
        
        case AST_FIELD_DECL:
            if (node->as.field_decl.name) free(node->as.field_decl.name);
            if (node->as.field_decl.type_name) free(node->as.field_decl.type_name);
            if (node->as.field_decl.initializer) xr_ast_free(X, node->as.field_decl.initializer);
            break;
        
        case AST_METHOD_DECL:
            if (node->as.method_decl.name) free(node->as.method_decl.name);
            if (node->as.method_decl.return_type) free(node->as.method_decl.return_type);
            if (node->as.method_decl.parameters) {
                for (int i = 0; i < node->as.method_decl.param_count; i++) {
                    if (node->as.method_decl.parameters[i]) free(node->as.method_decl.parameters[i]);
                    if (node->as.method_decl.param_types && node->as.method_decl.param_types[i]) {
                        free(node->as.method_decl.param_types[i]);
                    }
                }
                free(node->as.method_decl.parameters);
                if (node->as.method_decl.param_types) free(node->as.method_decl.param_types);
            }
            if (node->as.method_decl.body) xr_ast_free(X, node->as.method_decl.body);
            break;
        
        case AST_NEW_EXPR:
            if (node->as.new_expr.class_name) free(node->as.new_expr.class_name);
            if (node->as.new_expr.arguments) {
                for (int i = 0; i < node->as.new_expr.arg_count; i++) {
                    xr_ast_free(X, node->as.new_expr.arguments[i]);
                }
                free(node->as.new_expr.arguments);
            }
            break;
        
        case AST_THIS_EXPR:
            /* this不需要释放额外数据 */
            break;
        
        case AST_SUPER_CALL:
            if (node->as.super_call.method_name) free(node->as.super_call.method_name);
            if (node->as.super_call.arguments) {
                for (int i = 0; i < node->as.super_call.arg_count; i++) {
                    xr_ast_free(X, node->as.super_call.arguments[i]);
                }
                free(node->as.super_call.arguments);
            }
            break;
        
        case AST_MEMBER_SET:
            if (node->as.member_set.member) free(node->as.member_set.member);
            if (node->as.member_set.object) xr_ast_free(X, node->as.member_set.object);
            if (node->as.member_set.value) xr_ast_free(X, node->as.member_set.value);
            break;
        
        case AST_INDEX_GET:
            /* 释放数组和索引表达式 */
            xr_ast_free(X, node->as.index_get.array);
            xr_ast_free(X, node->as.index_get.index);
            break;
        
        case AST_INDEX_SET:
            /* 释放数组、索引和值表达式 */
            xr_ast_free(X, node->as.index_set.array);
            xr_ast_free(X, node->as.index_set.index);
            xr_ast_free(X, node->as.index_set.value);
            break;
        
        case AST_MEMBER_ACCESS:
            /* 释放对象表达式和成员名 */
            xr_ast_free(X, node->as.member_access.object);
            if (node->as.member_access.name != NULL) {
                free(node->as.member_access.name);
            }
            break;
        
        /* 程序节点 */
        case AST_PROGRAM:
            for (int i = 0; i < node->as.program.count; i++) {
                xr_ast_free(X, node->as.program.statements[i]);
            }
            if (node->as.program.statements != NULL) {
                free(node->as.program.statements);
            }
            break;
    }
    
    /* 释放节点本身 */
    free(node);
}

/* ========== 调试和工具函数 ========== */

/*
** 获取节点类型名称
** 用于调试和错误报告
*/
const char *xr_ast_typename(AstNodeType type) {
    switch (type) {
        case AST_LITERAL_INT:       return "LiteralInt";
        case AST_LITERAL_FLOAT:     return "LiteralFloat";
        case AST_LITERAL_STRING:    return "LiteralString";
        case AST_LITERAL_NULL:      return "LiteralNull";
        case AST_LITERAL_TRUE:      return "LiteralTrue";
        case AST_LITERAL_FALSE:     return "LiteralFalse";
        case AST_BINARY_ADD:        return "BinaryAdd";
        case AST_BINARY_SUB:        return "BinarySub";
        case AST_BINARY_MUL:        return "BinaryMul";
        case AST_BINARY_DIV:        return "BinaryDiv";
        case AST_BINARY_MOD:        return "BinaryMod";
        case AST_BINARY_EQ:         return "BinaryEq";
        case AST_BINARY_NE:         return "BinaryNe";
        case AST_BINARY_LT:         return "BinaryLt";
        case AST_BINARY_LE:         return "BinaryLe";
        case AST_BINARY_GT:         return "BinaryGt";
        case AST_BINARY_GE:         return "BinaryGe";
        case AST_BINARY_AND:        return "BinaryAnd";
        case AST_BINARY_OR:         return "BinaryOr";
        case AST_UNARY_NEG:         return "UnaryNeg";
        case AST_UNARY_NOT:         return "UnaryNot";
        case AST_GROUPING:          return "Grouping";
        case AST_EXPR_STMT:         return "ExprStmt";
        case AST_PRINT_STMT:        return "PrintStmt";
        case AST_BLOCK:             return "Block";
        case AST_VAR_DECL:          return "VarDecl";
        case AST_CONST_DECL:        return "ConstDecl";
        case AST_VARIABLE:          return "Variable";
        case AST_ASSIGNMENT:        return "Assignment";
        case AST_IF_STMT:           return "IfStmt";
        case AST_WHILE_STMT:        return "WhileStmt";
        case AST_FOR_STMT:          return "ForStmt";
        case AST_BREAK_STMT:        return "BreakStmt";
        case AST_CONTINUE_STMT:     return "ContinueStmt";
        case AST_FUNCTION_DECL:     return "FunctionDecl";
        case AST_CALL_EXPR:         return "CallExpr";
        case AST_RETURN_STMT:       return "ReturnStmt";
        case AST_ARRAY_LITERAL:     return "ArrayLiteral";
        case AST_MAP_LITERAL:       return "MapLiteral";
        case AST_INDEX_GET:         return "IndexGet";
        case AST_INDEX_SET:         return "IndexSet";
        case AST_MEMBER_ACCESS:     return "MemberAccess";
        case AST_TEMPLATE_STRING:   return "TemplateString";
        case AST_CLASS_DECL:        return "ClassDecl";
        case AST_FIELD_DECL:        return "FieldDecl";
        case AST_METHOD_DECL:       return "MethodDecl";
        case AST_NEW_EXPR:          return "NewExpr";
        case AST_THIS_EXPR:         return "ThisExpr";
        case AST_SUPER_CALL:        return "SuperCall";
        case AST_MEMBER_SET:        return "MemberSet";
        case AST_PROGRAM:           return "Program";
        default:                    return "Unknown";
    }
}

/*
** 打印 AST 结构（用于调试）
** indent: 缩进级别
*/
void xr_ast_print(AstNode *node, int indent) {
    if (node == NULL) {
        printf("%*sNULL\n", indent * 2, "");
        return;
    }
    
    /* 打印缩进 */
    printf("%*s", indent * 2, "");
    
    /* 打印节点类型 */
    printf("%s", xr_ast_typename(node->type));
    
    /* 打印节点详细信息 */
    switch (node->type) {
        case AST_LITERAL_INT:
            printf("(%lld)", (long long)xr_toint(node->as.literal.value));  /* 新API：不需要& */
            break;
        case AST_LITERAL_FLOAT:
            printf("(%g)", xr_tofloat(node->as.literal.value));  /* 新API：不需要& */
            break;
        case AST_LITERAL_STRING:
            printf("(\"%s\")", (char *)node->as.literal.value.as.obj);
            break;
        case AST_LITERAL_NULL:
            printf("(null)");
            break;
        case AST_LITERAL_TRUE:
            printf("(true)");
            break;
        case AST_LITERAL_FALSE:
            printf("(false)");
            break;
        default:
            break;
    }
    
    printf("\n");
    
    /* 递归打印子节点 */
    switch (node->type) {
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
            xr_ast_print(node->as.binary.left, indent + 1);
            xr_ast_print(node->as.binary.right, indent + 1);
            break;
        
        case AST_UNARY_NEG:
        case AST_UNARY_NOT:
            xr_ast_print(node->as.unary.operand, indent + 1);
            break;
        
        case AST_GROUPING:
            xr_ast_print(node->as.grouping, indent + 1);
            break;
        
        case AST_EXPR_STMT:
            xr_ast_print(node->as.expr_stmt, indent + 1);
            break;
        
        case AST_PRINT_STMT:
            xr_ast_print(node->as.print_stmt.expr, indent + 1);
            break;
        
        case AST_BLOCK:
            for (int i = 0; i < node->as.block.count; i++) {
                xr_ast_print(node->as.block.statements[i], indent + 1);
            }
            break;
        
        case AST_VAR_DECL:
        case AST_CONST_DECL:
            printf("%*s  name: %s\n", indent * 2, "", node->as.var_decl.name);
            if (node->as.var_decl.initializer != NULL) {
                printf("%*s  initializer:\n", indent * 2, "");
                xr_ast_print(node->as.var_decl.initializer, indent + 2);
            }
            break;
        
        case AST_VARIABLE:
            printf("%*s  name: %s\n", indent * 2, "", node->as.variable.name);
            break;
        
        case AST_ASSIGNMENT:
            printf("%*s  name: %s\n", indent * 2, "", node->as.assignment.name);
            printf("%*s  value:\n", indent * 2, "");
            xr_ast_print(node->as.assignment.value, indent + 2);
            break;
        
        case AST_IF_STMT:
            printf("%*s  condition:\n", indent * 2, "");
            xr_ast_print(node->as.if_stmt.condition, indent + 2);
            printf("%*s  then:\n", indent * 2, "");
            xr_ast_print(node->as.if_stmt.then_branch, indent + 2);
            if (node->as.if_stmt.else_branch != NULL) {
                printf("%*s  else:\n", indent * 2, "");
                xr_ast_print(node->as.if_stmt.else_branch, indent + 2);
            }
            break;
        
        case AST_WHILE_STMT:
            printf("%*s  condition:\n", indent * 2, "");
            xr_ast_print(node->as.while_stmt.condition, indent + 2);
            printf("%*s  body:\n", indent * 2, "");
            xr_ast_print(node->as.while_stmt.body, indent + 2);
            break;
        
        case AST_FOR_STMT:
            if (node->as.for_stmt.initializer != NULL) {
                printf("%*s  initializer:\n", indent * 2, "");
                xr_ast_print(node->as.for_stmt.initializer, indent + 2);
            }
            if (node->as.for_stmt.condition != NULL) {
                printf("%*s  condition:\n", indent * 2, "");
                xr_ast_print(node->as.for_stmt.condition, indent + 2);
            }
            if (node->as.for_stmt.increment != NULL) {
                printf("%*s  increment:\n", indent * 2, "");
                xr_ast_print(node->as.for_stmt.increment, indent + 2);
            }
            printf("%*s  body:\n", indent * 2, "");
            xr_ast_print(node->as.for_stmt.body, indent + 2);
            break;
        
        case AST_BREAK_STMT:
        case AST_CONTINUE_STMT:
            /* 无需打印额外信息 */
            break;
        
        /* 函数相关节点 */
        case AST_FUNCTION_DECL:
            printf(" (name: %s, params: ", node->as.function_decl.name);
            for (int i = 0; i < node->as.function_decl.param_count; i++) {
                printf("%s", node->as.function_decl.parameters[i]);
                if (i < node->as.function_decl.param_count - 1) {
                    printf(", ");
                }
            }
            printf(")\n");
            if (node->as.function_decl.body != NULL) {
                xr_ast_print(node->as.function_decl.body, indent + 1);
            }
            break;
        
        case AST_CALL_EXPR:
            printf("\n");
            /* 打印被调用者 */
            if (node->as.call_expr.callee != NULL) {
                xr_ast_print(node->as.call_expr.callee, indent + 1);
            }
            /* 打印参数 */
            for (int i = 0; i < node->as.call_expr.arg_count; i++) {
                xr_ast_print(node->as.call_expr.arguments[i], indent + 1);
            }
            break;
        
        case AST_RETURN_STMT:
            printf("\n");
            if (node->as.return_stmt.value != NULL) {
                xr_ast_print(node->as.return_stmt.value, indent + 1);
            }
            break;
        
        /* 数组相关节点 */
        case AST_ARRAY_LITERAL:
            printf(" [%d elements]\n", node->as.array_literal.count);
            for (int i = 0; i < node->as.array_literal.count; i++) {
                printf("%*s", (indent + 1) * 2, "");
                printf("Element %d:", i);
                xr_ast_print(node->as.array_literal.elements[i], indent + 2);
            }
            break;
        
        /* Map相关节点（v0.11.0）*/
        case AST_MAP_LITERAL:
            printf(" {%d pairs}\n", node->as.map_literal.count);
            for (int i = 0; i < node->as.map_literal.count; i++) {
                printf("%*s", (indent + 1) * 2, "");
                printf("Key %d:", i);
                xr_ast_print(node->as.map_literal.keys[i], indent + 2);
                printf("%*s", (indent + 1) * 2, "");
                printf("Value %d:", i);
                xr_ast_print(node->as.map_literal.values[i], indent + 2);
            }
            break;
        
        case AST_INDEX_GET:
            printf("\n");
            printf("%*s", (indent + 1) * 2, "");
            printf("Array:");
            xr_ast_print(node->as.index_get.array, indent + 2);
            printf("%*s", (indent + 1) * 2, "");
            printf("Index:");
            xr_ast_print(node->as.index_get.index, indent + 2);
            break;
        
        case AST_INDEX_SET:
            printf("\n");
            printf("%*s", (indent + 1) * 2, "");
            printf("Array:");
            xr_ast_print(node->as.index_set.array, indent + 2);
            printf("%*s", (indent + 1) * 2, "");
            printf("Index:");
            xr_ast_print(node->as.index_set.index, indent + 2);
            printf("%*s", (indent + 1) * 2, "");
            printf("Value:");
            xr_ast_print(node->as.index_set.value, indent + 2);
            break;
        
        case AST_MEMBER_ACCESS:
            printf(" .%s\n", node->as.member_access.name);
            printf("%*s", (indent + 1) * 2, "");
            printf("Object:");
            xr_ast_print(node->as.member_access.object, indent + 2);
            break;
        
        case AST_PROGRAM:
            for (int i = 0; i < node->as.program.count; i++) {
                xr_ast_print(node->as.program.statements[i], indent + 1);
            }
            break;
        
        default:
            break;
    }
}


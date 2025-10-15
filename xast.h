/*
** xast.h
** Xray 抽象语法树（AST）定义
** 定义了表达式和语句的 AST 节点结构
*/

#ifndef xast_h
#define xast_h

#include "xray.h"
#include "xvalue.h"
#include <stdbool.h>

/* 前向声明 */
typedef struct AstNode AstNode;

/*
** AST 节点类型
** 定义了所有支持的表达式和语句节点
*/
typedef enum {
    /* 字面量节点 */
    AST_LITERAL_INT,        /* 整数字面量：123 */
    AST_LITERAL_FLOAT,      /* 浮点数字面量：3.14 */
    AST_LITERAL_STRING,     /* 字符串字面量："hello" */
    AST_LITERAL_NULL,       /* null */
    AST_LITERAL_TRUE,       /* true */
    AST_LITERAL_FALSE,      /* false */
    
    /* 模板字符串（v0.10.0 Day 5新增）*/
    AST_TEMPLATE_STRING,    /* 模板字符串：`Hello, ${name}!` */
    
    /* 二元运算节点 - 算术运算 */
    AST_BINARY_ADD,         /* 加法：a + b */
    AST_BINARY_SUB,         /* 减法：a - b */
    AST_BINARY_MUL,         /* 乘法：a * b */
    AST_BINARY_DIV,         /* 除法：a / b */
    AST_BINARY_MOD,         /* 取模：a % b */
    
    /* 二元运算节点 - 比较运算 */
    AST_BINARY_EQ,          /* 相等：a == b */
    AST_BINARY_NE,          /* 不等：a != b */
    AST_BINARY_LT,          /* 小于：a < b */
    AST_BINARY_LE,          /* 小于等于：a <= b */
    AST_BINARY_GT,          /* 大于：a > b */
    AST_BINARY_GE,          /* 大于等于：a >= b */
    
    /* 二元运算节点 - 逻辑运算 */
    AST_BINARY_AND,         /* 逻辑与：a && b */
    AST_BINARY_OR,          /* 逻辑或：a || b */
    
    /* 一元运算节点 */
    AST_UNARY_NEG,          /* 取负：-a */
    AST_UNARY_NOT,          /* 逻辑非：!a */
    
    /* 分组节点 */
    AST_GROUPING,           /* 括号分组：(expr) */
    
    /* 语句节点 */
    AST_EXPR_STMT,          /* 表达式语句 */
    AST_PRINT_STMT,         /* print 语句（内置） */
    AST_BLOCK,              /* 代码块：{ ... } */
    
    /* 变量相关节点 */
    AST_VAR_DECL,           /* 变量声明：let x = 10 */
    AST_CONST_DECL,         /* 常量声明：const PI = 3.14 */
    AST_VARIABLE,           /* 变量引用：x */
    AST_ASSIGNMENT,         /* 赋值：x = 10 */
    
    /* 控制流节点 */
    AST_IF_STMT,            /* if 语句：if (cond) {...} else {...} */
    AST_WHILE_STMT,         /* while 循环：while (cond) {...} */
    AST_FOR_STMT,           /* for 循环：for (init; cond; update) {...} */
    AST_BREAK_STMT,         /* break 语句 */
    AST_CONTINUE_STMT,      /* continue 语句 */
    
    /* 函数相关节点 */
    AST_FUNCTION_DECL,      /* 函数声明：function add(a, b) {...} */
    AST_FUNCTION_EXPR,      /* 函数表达式/箭头函数：(a, b) => a + b */
    AST_CALL_EXPR,          /* 函数调用：add(1, 2) */
    AST_RETURN_STMT,        /* return 语句：return expr */
    
    /* 数组相关节点 */
    AST_ARRAY_LITERAL,      /* 数组字面量：[1, 2, 3] */
    AST_INDEX_GET,          /* 索引访问：arr[0] */
    AST_INDEX_SET,          /* 索引赋值：arr[0] = 10 */
    AST_MEMBER_ACCESS,      /* 成员访问：arr.length, arr.push */
    
    /* Map相关节点 */
    AST_MAP_LITERAL,        /* Map字面量：{a: 1, b: 2} */
    
    /* OOP相关节点（v0.12.0新增）*/
    AST_CLASS_DECL,         /* class声明：class Dog extends Animal {...} */
    AST_FIELD_DECL,         /* 字段声明：name: string */
    AST_METHOD_DECL,        /* 方法声明：greet() {...} */
    AST_NEW_EXPR,           /* new表达式：new Dog("Rex") */
    AST_THIS_EXPR,          /* this表达式 */
    AST_SUPER_CALL,         /* super调用：super.greet() 或 super(args) */
    AST_MEMBER_SET,         /* 成员赋值：obj.field = value */
    
    /* 程序节点 */
    AST_PROGRAM             /* 程序根节点 */
} AstNodeType;

/*
** 二元运算节点
** 包含左右两个操作数
*/
typedef struct {
    AstNode *left;          /* 左操作数 */
    AstNode *right;         /* 右操作数 */
} BinaryNode;

/*
** 一元运算节点
** 包含一个操作数
*/
typedef struct {
    AstNode *operand;       /* 操作数 */
} UnaryNode;

/*
** 字面量节点
** 存储字面量的值
*/
typedef struct {
    XrValue value;          /* 字面量的值 */
} LiteralNode;

/*
** 程序节点
** 包含语句列表
*/
typedef struct {
    AstNode **statements;   /* 语句数组 */
    int count;              /* 语句数量 */
    int capacity;           /* 数组容量 */
} ProgramNode;

/*
** print 语句节点
** 包含要打印的表达式
*/
typedef struct {
    AstNode *expr;          /* 要打印的表达式 */
} PrintNode;

/*
** 代码块节点
** 包含一系列语句
*/
typedef struct {
    AstNode **statements;   /* 语句数组 */
    int count;              /* 语句数量 */
    int capacity;           /* 数组容量 */
} BlockNode;

/*
** 变量声明节点
** let x = 10 或 let x（无初始化）
*/
typedef struct {
    char *name;             /* 变量名 */
    AstNode *initializer;   /* 初始化表达式（可选） */
    bool is_const;          /* 是否为常量 */
} VarDeclNode;

/*
** 变量引用节点
** 引用一个变量：x
*/
typedef struct {
    char *name;             /* 变量名 */
} VariableNode;

/*
** 赋值节点
** x = 10
*/
typedef struct {
    char *name;             /* 变量名 */
    AstNode *value;         /* 赋值表达式 */
} AssignmentNode;

/*
** if 语句节点
** if (condition) then_branch else else_branch
*/
typedef struct {
    AstNode *condition;     /* 条件表达式 */
    AstNode *then_branch;   /* then 分支（必须是 block） */
    AstNode *else_branch;   /* else 分支（可选，可以是 block 或 if） */
} IfStmtNode;

/*
** while 循环节点
** while (condition) body
*/
typedef struct {
    AstNode *condition;     /* 循环条件 */
    AstNode *body;          /* 循环体（必须是 block） */
} WhileStmtNode;

/*
** for 循环节点
** for (initializer; condition; increment) body
*/
typedef struct {
    AstNode *initializer;   /* 初始化（可选，可以是 VarDecl 或表达式） */
    AstNode *condition;     /* 条件（可选） */
    AstNode *increment;     /* 更新（可选） */
    AstNode *body;          /* 循环体（必须是 block） */
} ForStmtNode;

/*
** break 语句节点
** break
*/
typedef struct {
    int placeholder;        /* 占位符（break 不需要额外数据） */
} BreakStmtNode;

/*
** continue 语句节点
** continue
*/
typedef struct {
    int placeholder;        /* 占位符（continue 不需要额外数据） */
} ContinueStmtNode;

/*
** 函数声明节点
** function add(a, b) { return a + b }
*/
typedef struct {
    char *name;             /* 函数名 */
    char **parameters;      /* 参数列表 */
    int param_count;        /* 参数数量 */
    AstNode *body;          /* 函数体（必须是 block） */
} FunctionDeclNode;

/*
** 函数调用节点
** add(1, 2)
*/
typedef struct {
    AstNode *callee;        /* 被调用的表达式（通常是变量） */
    AstNode **arguments;    /* 参数列表 */
    int arg_count;          /* 参数数量 */
} CallExprNode;

/*
** return 语句节点
** return expr 或 return
*/
typedef struct {
    AstNode *value;         /* 返回值表达式（可选） */
} ReturnStmtNode;

/*
** 数组字面量节点
** [1, 2, 3]
*/
typedef struct {
    AstNode **elements;     /* 元素表达式数组 */
    int count;              /* 元素数量 */
} ArrayLiteralNode;

/*
** 索引访问节点
** arr[0]
*/
typedef struct {
    AstNode *array;         /* 数组表达式 */
    AstNode *index;         /* 索引表达式 */
} IndexGetNode;

/*
** 索引赋值节点
** arr[0] = 10
*/
typedef struct {
    AstNode *array;         /* 数组表达式 */
    AstNode *index;         /* 索引表达式 */
    AstNode *value;         /* 赋值表达式 */
} IndexSetNode;

/*
** 成员访问节点
** arr.length, arr.push
*/
typedef struct {
    AstNode *object;        /* 对象表达式 */
    char *name;             /* 成员名称 */
} MemberAccessNode;

/*
** 模板字符串节点（v0.10.0 Day 5新增）
** `Hello, ${name}! ${age}`
** 
** 存储：交替存储字符串片段和表达式
** 例如: `Hello, ${name}!` 
** parts = ["Hello, ", expr(name), "!"]
*/
typedef struct {
    AstNode **parts;        /* 字符串片段和表达式（交替）*/
    int part_count;         /* 片段数量 */
} TemplateStringNode;

/*
** Map字面量节点（v0.11.0 第11阶段新增）
** {name: "Alice", age: 30}
*/
typedef struct {
    AstNode **keys;         /* 键表达式数组 */
    AstNode **values;       /* 值表达式数组 */
    int count;              /* 键值对数量 */
} MapLiteralNode;

/*
** 类声明节点（v0.12.0 第12阶段新增）
** class Dog extends Animal { ... }
*/
typedef struct {
    char *name;             /* 类名："Dog" */
    char *super_name;       /* 超类名："Animal"（可选）*/
    AstNode **fields;       /* 字段声明数组 */
    int field_count;        /* 字段数量 */
    AstNode **methods;      /* 方法声明数组 */
    int method_count;       /* 方法数量 */
} ClassDeclNode;

/*
** 字段声明节点（v0.12.0）
** name: string 或 private age: int
*/
typedef struct {
    char *name;             /* 字段名 */
    char *type_name;        /* 类型名（可选）*/
    bool is_private;        /* 是否私有 */
    bool is_static;         /* 是否静态 */
    AstNode *initializer;   /* 初始值（可选）*/
} FieldDeclNode;

/*
** 方法声明节点（v0.12.0）
** greet() { ... } 或 constructor(name) { ... }
*/
typedef struct {
    char *name;             /* 方法名 */
    char **parameters;      /* 参数列表 */
    char **param_types;     /* 参数类型列表（可选）*/
    int param_count;        /* 参数数量 */
    char *return_type;      /* 返回类型（可选）*/
    AstNode *body;          /* 方法体 */
    bool is_constructor;    /* 是否构造函数 */
    bool is_static;         /* 是否静态方法 */
    bool is_private;        /* 是否私有方法 */
    bool is_getter;         /* 是否getter */
    bool is_setter;         /* 是否setter */
} MethodDeclNode;

/*
** new表达式节点（v0.12.0）
** new Dog("Rex", "Labrador")
*/
typedef struct {
    char *class_name;       /* 类名 */
    AstNode **arguments;    /* 构造参数 */
    int arg_count;          /* 参数数量 */
} NewExprNode;

/*
** this表达式节点（v0.12.0）
** this
*/
typedef struct {
    int placeholder;        /* this不需要额外数据 */
} ThisExprNode;

/*
** super调用节点（v0.12.0）
** super.greet() 或 super(args)
*/
typedef struct {
    char *method_name;      /* 方法名（NULL表示super构造函数）*/
    AstNode **arguments;    /* 参数列表 */
    int arg_count;          /* 参数数量 */
} SuperCallNode;

/*
** 成员赋值节点（v0.12.0）
** obj.field = value
*/
typedef struct {
    AstNode *object;        /* 对象表达式 */
    char *member;           /* 成员名 */
    AstNode *value;         /* 赋值表达式 */
} MemberSetNode;

/*
** AST 节点通用结构
** 所有节点类型的基础
*/
struct AstNode {
    AstNodeType type;       /* 节点类型 */
    int line;               /* 源代码行号（用于错误报告） */
    
    /* 节点数据（根据类型使用不同的字段） */
    union {
        LiteralNode literal;        /* 字面量 */
        BinaryNode binary;          /* 二元运算 */
        UnaryNode unary;            /* 一元运算 */
        AstNode *grouping;          /* 分组表达式 */
        AstNode *expr_stmt;         /* 表达式语句 */
        PrintNode print_stmt;       /* print 语句 */
        BlockNode block;            /* 代码块 */
        VarDeclNode var_decl;       /* 变量声明 */
        VariableNode variable;      /* 变量引用 */
        AssignmentNode assignment;  /* 赋值 */
        IfStmtNode if_stmt;         /* if 语句 */
        WhileStmtNode while_stmt;   /* while 循环 */
        ForStmtNode for_stmt;       /* for 循环 */
        BreakStmtNode break_stmt;   /* break 语句 */
        ContinueStmtNode continue_stmt; /* continue 语句 */
        FunctionDeclNode function_decl; /* 函数声明 */
        FunctionDeclNode function_expr; /* 函数表达式（复用FunctionDeclNode，name可为NULL） */
        CallExprNode call_expr;     /* 函数调用 */
        ReturnStmtNode return_stmt; /* return 语句 */
        ArrayLiteralNode array_literal;  /* 数组字面量 */
        IndexGetNode index_get;     /* 索引访问 */
        IndexSetNode index_set;     /* 索引赋值 */
        MemberAccessNode member_access; /* 成员访问 */
        TemplateStringNode template_str;  /* 模板字符串（v0.10.0）*/
        MapLiteralNode map_literal;  /* Map字面量（v0.11.0）*/
        ClassDeclNode class_decl;   /* 类声明（v0.12.0）*/
        FieldDeclNode field_decl;   /* 字段声明（v0.12.0）*/
        MethodDeclNode method_decl; /* 方法声明（v0.12.0）*/
        NewExprNode new_expr;       /* new表达式（v0.12.0）*/
        ThisExprNode this_expr;     /* this表达式（v0.12.0）*/
        SuperCallNode super_call;   /* super调用（v0.12.0）*/
        MemberSetNode member_set;   /* 成员赋值（v0.12.0）*/
        ProgramNode program;        /* 程序 */
    } as;
};

/* ========== AST 节点创建函数 ========== */

/* 创建字面量节点 */
AstNode *xr_ast_literal_int(XrayState *X, xr_Integer value, int line);
AstNode *xr_ast_literal_float(XrayState *X, xr_Number value, int line);
AstNode *xr_ast_literal_string(XrayState *X, const char *value, int line);
AstNode *xr_ast_literal_null(XrayState *X, int line);
AstNode *xr_ast_literal_bool(XrayState *X, int value, int line);

/* 创建模板字符串节点（v0.10.0 Day 5新增）*/
AstNode *xr_ast_template_string(XrayState *X, AstNode **parts, int part_count, int line);

/* 创建二元运算节点 */
AstNode *xr_ast_binary(XrayState *X, AstNodeType type, 
                       AstNode *left, AstNode *right, int line);

/* 创建一元运算节点 */
AstNode *xr_ast_unary(XrayState *X, AstNodeType type, 
                      AstNode *operand, int line);

/* 创建分组节点 */
AstNode *xr_ast_grouping(XrayState *X, AstNode *expr, int line);

/* 创建表达式语句节点 */
AstNode *xr_ast_expr_stmt(XrayState *X, AstNode *expr, int line);

/* 创建 print 语句节点 */
AstNode *xr_ast_print_stmt(XrayState *X, AstNode *expr, int line);

/* 创建程序节点 */
AstNode *xr_ast_program(XrayState *X);

/* 向程序节点添加语句 */
void xr_ast_program_add(XrayState *X, AstNode *program, AstNode *stmt);

/* 创建代码块节点 */
AstNode *xr_ast_block(XrayState *X, int line);

/* 向代码块添加语句 */
void xr_ast_block_add(XrayState *X, AstNode *block, AstNode *stmt);

/* 创建变量声明节点 */
AstNode *xr_ast_var_decl(XrayState *X, const char *name, 
                         AstNode *initializer, bool is_const, int line);

/* 创建变量引用节点 */
AstNode *xr_ast_variable(XrayState *X, const char *name, int line);

/* 创建赋值节点 */
AstNode *xr_ast_assignment(XrayState *X, const char *name, 
                           AstNode *value, int line);

/* 创建 if 语句节点 */
AstNode *xr_ast_if_stmt(XrayState *X, AstNode *condition, 
                        AstNode *then_branch, AstNode *else_branch, int line);

/* 创建 while 循环节点 */
AstNode *xr_ast_while_stmt(XrayState *X, AstNode *condition, 
                           AstNode *body, int line);

/* 创建 for 循环节点 */
AstNode *xr_ast_for_stmt(XrayState *X, AstNode *initializer, 
                         AstNode *condition, AstNode *increment, 
                         AstNode *body, int line);

/* 创建 break 语句节点 */
AstNode *xr_ast_break_stmt(XrayState *X, int line);

/* 创建 continue 语句节点 */
AstNode *xr_ast_continue_stmt(XrayState *X, int line);

/* 创建函数声明节点 */
AstNode *xr_ast_function_decl(XrayState *X, const char *name,
                              char **parameters, int param_count,
                              AstNode *body, int line);

/* 创建函数表达式节点（箭头函数/匿名函数） */
AstNode *xr_ast_function_expr(XrayState *X, char **parameters, int param_count,
                              AstNode *body, int line);

/* 创建函数调用节点 */
AstNode *xr_ast_call_expr(XrayState *X, AstNode *callee, 
                          AstNode **arguments, int arg_count, int line);

/* 创建 return 语句节点 */
AstNode *xr_ast_return_stmt(XrayState *X, AstNode *value, int line);

/* 创建数组字面量节点 */
AstNode *xr_ast_array_literal(XrayState *X, AstNode **elements, int count, int line);

/* 创建Map字面量节点（v0.11.0） */
AstNode *xr_ast_map_literal(XrayState *X, AstNode **keys, AstNode **values, int count, int line);

/* 创建索引访问节点 */
AstNode *xr_ast_index_get(XrayState *X, AstNode *array, AstNode *index, int line);

/* 创建索引赋值节点 */
AstNode *xr_ast_index_set(XrayState *X, AstNode *array, AstNode *index, AstNode *value, int line);

/* 创建成员访问节点 */
AstNode *xr_ast_member_access(XrayState *X, AstNode *object, const char *name, int line);

/* ========== OOP AST节点创建函数（v0.12.0）========== */

/* 创建类声明节点 */
AstNode *xr_ast_class_decl(XrayState *X, const char *name, const char *super_name,
                           AstNode **fields, int field_count,
                           AstNode **methods, int method_count, int line);

/* 创建字段声明节点 */
AstNode *xr_ast_field_decl(XrayState *X, const char *name, const char *type_name,
                           bool is_private, bool is_static,
                           AstNode *initializer, int line);

/* 创建方法声明节点 */
AstNode *xr_ast_method_decl(XrayState *X, const char *name,
                            char **parameters, char **param_types, int param_count,
                            const char *return_type, AstNode *body,
                            bool is_constructor, bool is_static, bool is_private,
                            bool is_getter, bool is_setter, int line);

/* 创建new表达式节点 */
AstNode *xr_ast_new_expr(XrayState *X, const char *class_name,
                         AstNode **arguments, int arg_count, int line);

/* 创建this表达式节点 */
AstNode *xr_ast_this_expr(XrayState *X, int line);

/* 创建super调用节点 */
AstNode *xr_ast_super_call(XrayState *X, const char *method_name,
                           AstNode **arguments, int arg_count, int line);

/* 创建成员赋值节点 */
AstNode *xr_ast_member_set(XrayState *X, AstNode *object, const char *member,
                           AstNode *value, int line);

/* 释放 AST 节点 */
void xr_ast_free(XrayState *X, AstNode *node);

/* 调试：打印 AST 结构（用于调试和测试） */
void xr_ast_print(AstNode *node, int indent);

/* 获取节点类型名称（用于调试） */
const char *xr_ast_typename(AstNodeType type);

#endif /* xast_h */


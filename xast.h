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

/* 释放 AST 节点 */
void xr_ast_free(XrayState *X, AstNode *node);

/* 调试：打印 AST 结构（用于调试和测试） */
void xr_ast_print(AstNode *node, int indent);

/* 获取节点类型名称（用于调试） */
const char *xr_ast_typename(AstNodeType type);

#endif /* xast_h */


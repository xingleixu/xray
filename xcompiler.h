/*
** xcompiler.h
** Xray 寄存器编译器 - AST到字节码
** 
** v0.13.0: 寄存器分配和代码生成
** 创建日期: 2025-10-15
*/

#ifndef xcompiler_h
#define xcompiler_h

#include "xchunk.h"
#include "xast.h"
#include "xstring.h"
#include <stdbool.h>

/* ========== 常量定义 ========== */

#define MAXREGS 250     /* 最大寄存器数量（参考Lua） */

/* ========== 函数类型 ========== */

typedef enum {
    FUNCTION_SCRIPT,    /* 顶层脚本 */
    FUNCTION_FUNCTION,  /* 普通函数 */
} FunctionType;

/* ========== 局部变量 ========== */

/* 局部变量描述 */
typedef struct {
    XrString *name;     /* 变量名 */
    int reg;            /* 寄存器编号 */
    int depth;          /* 作用域深度 */
    bool is_captured;   /* 是否被闭包捕获 */
} Local;

/* ========== Upvalue描述 ========== */

/* Upvalue描述（编译时） */
typedef struct {
    uint8_t index;      /* 索引（局部变量reg或外层upvalue索引） */
    bool is_local;      /* 是否是局部变量 */
} Upvalue;

/* ========== 寄存器状态 ========== */

/* 寄存器分配状态 */
typedef struct {
    int freereg;        /* 下一个空闲寄存器 */
    int nactvar;        /* 活跃局部变量数量 */
} RegState;

/* ========== 编译器上下文 ========== */

/* 编译器状态 */
typedef struct Compiler {
    struct Compiler *enclosing;  /* 外层编译器（嵌套函数） */
    
    Proto *proto;                /* 当前函数原型 */
    FunctionType type;           /* 函数类型 */
    
    Local locals[MAXREGS];       /* 局部变量数组 */
    int local_count;             /* 局部变量数量 */
    
    Upvalue upvalues[UINT8_MAX]; /* upvalue数组 */
    
    int scope_depth;             /* 当前作用域深度 */
    
    RegState rs;                 /* 寄存器状态 */
    
    /* 循环控制 */
    int loop_depth;              /* 循环嵌套深度 */
    int loop_start;              /* 当前循环起始位置 */
    int loop_scope;              /* 当前循环作用域深度 */
    
    /* 错误标志 */
    bool had_error;
    bool panic_mode;
} Compiler;

/* ========== 编译API ========== */

/*
** 编译AST到函数原型
** @param ast AST根节点
** @return 编译后的函数原型，失败返回NULL
*/
Proto *xr_compile(AstNode *ast);

/* ========== 内部函数（暴露给测试）========== */

/*
** 初始化编译器
*/
void xr_compiler_init(Compiler *compiler, FunctionType type);

/*
** 结束编译
*/
Proto *xr_compiler_end(Compiler *compiler);

/*
** 编译语句
*/
void xr_compile_statement(Compiler *compiler, AstNode *node);

/*
** 编译表达式（返回结果寄存器）
*/
int xr_compile_expression(Compiler *compiler, AstNode *node);

/* ========== 寄存器分配 ========== */

/*
** 分配临时寄存器
*/
int xr_allocreg(Compiler *compiler);

/*
** 释放临时寄存器
*/
void xr_freereg(Compiler *compiler, int reg);

/*
** 保留寄存器（用于局部变量）
*/
void xr_reservereg(Compiler *compiler);

/* ========== 指令发射 ========== */

/*
** 发射ABC格式指令
*/
void xr_emit_ABC(Compiler *compiler, OpCode op, int a, int b, int c);

/*
** 发射ABx格式指令
*/
void xr_emit_ABx(Compiler *compiler, OpCode op, int a, int bx);

/*
** 发射AsBx格式指令
*/
void xr_emit_AsBx(Compiler *compiler, OpCode op, int a, int sbx);

/*
** 发射跳转指令（返回跳转位置，用于回填）
*/
int xr_emit_jump(Compiler *compiler, OpCode op);

/*
** 回填跳转偏移
*/
void xr_patch_jump(Compiler *compiler, int offset);

/*
** 发射循环指令（向后跳转）
*/
void xr_emit_loop(Compiler *compiler, int loop_start);

/* ========== 作用域管理 ========== */

/*
** 进入新作用域
*/
void xr_begin_scope(Compiler *compiler);

/*
** 离开作用域
*/
void xr_end_scope(Compiler *compiler);

/*
** 定义局部变量
*/
void xr_define_local(Compiler *compiler, XrString *name);

/*
** 解析局部变量（返回寄存器编号，-1表示未找到）
*/
int xr_resolve_local(Compiler *compiler, XrString *name);

/*
** 解析upvalue（返回upvalue索引，-1表示未找到）
*/
int xr_resolve_upvalue(Compiler *compiler, XrString *name);

/* ========== 错误处理 ========== */

/*
** 报告编译错误
*/
void xr_compiler_error(Compiler *compiler, const char *format, ...);

#endif /* xcompiler_h */


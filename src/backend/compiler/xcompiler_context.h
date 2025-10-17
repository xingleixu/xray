/*
** xcompiler_context.h
** Xray 编译器上下文 - 消除全局变量
** 
** 职责：
**   - 封装编译器的全局状态
**   - 支持多线程编译
**   - 便于测试和调试
** 
** v0.13.7: 新增编译器上下文
*/

#ifndef xcompiler_context_h
#define xcompiler_context_h

#include "xcompiler.h"
#include "xstring.h"

/* ========== 编译器上下文 ========== */

/*
** 编译器上下文
** 包含所有原本的全局变量
*/
struct CompilerContext {
    /* 原全局变量 */
    Compiler *current;              /* 当前编译器 */
    int current_line;               /* 当前行号 */
    GlobalVar *global_vars;         /* 全局变量数组 */
    int global_var_count;           /* 全局变量数量 */
    
    /* 扩展状态 */
    bool had_error;                 /* 是否有错误 */
    bool panic_mode;                /* 是否处于panic模式 */
    
    /* 资源管理 */
    int max_globals;                /* 最大全局变量数 */
};

/* ========== 上下文管理 ========== */

/*
** 创建编译器上下文
** @return 新的上下文，失败返回NULL
*/
CompilerContext* xr_compiler_context_new(void);

/*
** 释放编译器上下文
** @param ctx 要释放的上下文
*/
void xr_compiler_context_free(CompilerContext *ctx);

/*
** 重置编译器上下文
** @param ctx 要重置的上下文
*/
void xr_compiler_context_reset(CompilerContext *ctx);

/* ========== 全局变量操作 ========== */

/*
** 获取或添加全局变量
** @param ctx 编译器上下文
** @param name 变量名
** @return 全局变量索引，失败返回-1
*/
int xr_compiler_ctx_get_or_add_global(CompilerContext *ctx, XrString *name);

/*
** 查找全局变量
** @param ctx 编译器上下文
** @param name 变量名
** @return 全局变量索引，未找到返回-1
*/
int xr_compiler_ctx_find_global(CompilerContext *ctx, XrString *name);

/* ========== 错误处理（使用上下文）========== */

/*
** 设置错误状态
** @param ctx 编译器上下文
*/
void xr_compiler_ctx_set_error(CompilerContext *ctx);

/*
** 检查是否有错误
** @param ctx 编译器上下文
** @return true表示有错误
*/
bool xr_compiler_ctx_has_error(CompilerContext *ctx);

/* ========== 新的编译API（使用上下文）========== */

/*
** 编译AST到函数原型（使用上下文）
** @param ctx 编译器上下文
** @param ast AST根节点
** @return 编译后的函数原型，失败返回NULL
*/
Proto* xr_compile_with_context(CompilerContext *ctx, AstNode *ast);

#endif /* xcompiler_context_h */


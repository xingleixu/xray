/*
** xcompiler_context.c
** Xray 编译器上下文实现
*/

#include "xcompiler_context.h"
#include "xmem.h"
#include <string.h>
#include <stdio.h>

/*
** 创建编译器上下文
*/
CompilerContext* xr_compiler_context_new(void) {
    CompilerContext *ctx = (CompilerContext*)xmem_alloc(sizeof(CompilerContext));
    if (!ctx) {
        return NULL;
    }
    
    /* 初始化全局变量数组 */
    ctx->global_vars = (GlobalVar*)xmem_alloc(sizeof(GlobalVar) * MAX_GLOBALS);
    if (!ctx->global_vars) {
        xmem_free(ctx);
        return NULL;
    }
    
    /* 初始化状态 */
    ctx->current = NULL;
    ctx->current_line = 1;
    ctx->global_var_count = 0;
    ctx->had_error = false;
    ctx->panic_mode = false;
    ctx->max_globals = MAX_GLOBALS;
    
    return ctx;
}

/*
** 释放编译器上下文
*/
void xr_compiler_context_free(CompilerContext *ctx) {
    if (!ctx) return;
    
    /* 释放全局变量数组 */
    if (ctx->global_vars) {
        xmem_free(ctx->global_vars);
    }
    
    /* 释放上下文本身 */
    xmem_free(ctx);
}

/*
** 重置编译器上下文
*/
void xr_compiler_context_reset(CompilerContext *ctx) {
    if (!ctx) return;
    
    ctx->current = NULL;
    ctx->current_line = 1;
    ctx->global_var_count = 0;
    ctx->had_error = false;
    ctx->panic_mode = false;
}

/*
** 获取或添加全局变量
*/
int xr_compiler_ctx_get_or_add_global(CompilerContext *ctx, XrString *name) {
    if (!ctx || !name) return -1;
    
    /* 先查找是否已存在 */
    for (int i = 0; i < ctx->global_var_count; i++) {
        if (ctx->global_vars[i].name == name) {
            return ctx->global_vars[i].index;
        }
    }
    
    /* 检查是否超出限制 */
    if (ctx->global_var_count >= ctx->max_globals) {
        fprintf(stderr, "Error: Too many global variables (max %d)\n", ctx->max_globals);
        return -1;
    }
    
    /* 添加新的全局变量 */
    int index = ctx->global_var_count;
    ctx->global_vars[index].name = name;
    ctx->global_vars[index].index = index;
    ctx->global_var_count++;
    
    return index;
}

/*
** 查找全局变量
*/
int xr_compiler_ctx_find_global(CompilerContext *ctx, XrString *name) {
    if (!ctx || !name) return -1;
    
    for (int i = 0; i < ctx->global_var_count; i++) {
        if (ctx->global_vars[i].name == name) {
            return ctx->global_vars[i].index;
        }
    }
    
    return -1;
}

/*
** 设置错误状态
*/
void xr_compiler_ctx_set_error(CompilerContext *ctx) {
    if (ctx) {
        ctx->had_error = true;
    }
}

/*
** 检查是否有错误
*/
bool xr_compiler_ctx_has_error(CompilerContext *ctx) {
    return ctx ? ctx->had_error : false;
}

/*
** 编译AST到函数原型（使用上下文）
** 注意：此函数已废弃，请直接使用 xr_compile(ctx, ast)
*/
Proto* xr_compile_with_context(CompilerContext *ctx, AstNode *ast) {
    /* 直接转发到xr_compile */
    return xr_compile(ctx, ast);
}


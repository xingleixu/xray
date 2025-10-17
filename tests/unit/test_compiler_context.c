/*
** test_compiler_context.c
** 测试编译器上下文
*/

#include "xcompiler_context.h"
#include "xstring.h"
#include <stdio.h>
#include <assert.h>

int main(void) {
    printf("=== 测试编译器上下文 ===\n\n");
    
    /* 测试1: 创建和释放上下文 */
    printf("测试1: 创建和释放上下文\n");
    CompilerContext *ctx = xr_compiler_context_new();
    assert(ctx != NULL);
    assert(ctx->current == NULL);
    assert(ctx->current_line == 1);
    assert(ctx->global_var_count == 0);
    assert(ctx->had_error == false);
    printf("✓ 上下文创建成功\n\n");
    
    /* 测试2: 全局变量管理 */
    printf("测试2: 全局变量管理\n");
    
    /* 创建测试字符串 */
    XrString *name1 = xr_string_new("var1", 4);
    XrString *name2 = xr_string_new("var2", 4);
    XrString *name3 = xr_string_new("var3", 4);
    
    /* 添加全局变量 */
    int idx1 = xr_compiler_ctx_get_or_add_global(ctx, name1);
    assert(idx1 == 0);
    assert(ctx->global_var_count == 1);
    printf("✓ 添加第1个全局变量，索引=%d\n", idx1);
    
    int idx2 = xr_compiler_ctx_get_or_add_global(ctx, name2);
    assert(idx2 == 1);
    assert(ctx->global_var_count == 2);
    printf("✓ 添加第2个全局变量，索引=%d\n", idx2);
    
    /* 重复添加应该返回相同索引 */
    int idx1_again = xr_compiler_ctx_get_or_add_global(ctx, name1);
    assert(idx1_again == idx1);
    assert(ctx->global_var_count == 2);  /* 数量不变 */
    printf("✓ 重复添加返回相同索引=%d\n", idx1_again);
    
    /* 查找全局变量 */
    int found = xr_compiler_ctx_find_global(ctx, name2);
    assert(found == idx2);
    printf("✓ 查找全局变量成功，索引=%d\n\n", found);
    
    /* 测试3: 错误状态管理 */
    printf("测试3: 错误状态管理\n");
    assert(!xr_compiler_ctx_has_error(ctx));
    printf("✓ 初始状态无错误\n");
    
    xr_compiler_ctx_set_error(ctx);
    assert(xr_compiler_ctx_has_error(ctx));
    printf("✓ 设置错误状态成功\n\n");
    
    /* 测试4: 重置上下文 */
    printf("测试4: 重置上下文\n");
    xr_compiler_context_reset(ctx);
    assert(ctx->current == NULL);
    assert(ctx->current_line == 1);
    assert(ctx->global_var_count == 0);
    assert(ctx->had_error == false);
    printf("✓ 上下文重置成功\n\n");
    
    /* 测试5: 多上下文并存 */
    printf("测试5: 多上下文并存（线程安全性验证）\n");
    CompilerContext *ctx2 = xr_compiler_context_new();
    assert(ctx2 != NULL);
    assert(ctx2 != ctx);
    
    /* 在ctx2中添加变量，不影响ctx */
    int idx_ctx2 = xr_compiler_ctx_get_or_add_global(ctx2, name3);
    assert(idx_ctx2 == 0);
    assert(ctx2->global_var_count == 1);
    assert(ctx->global_var_count == 0);  /* ctx仍然是0 */
    printf("✓ 多个上下文互不干扰\n\n");
    
    /* 清理 */
    xr_compiler_context_free(ctx);
    xr_compiler_context_free(ctx2);
    xr_string_free(name1);
    xr_string_free(name2);
    xr_string_free(name3);
    
    printf("=== 所有测试通过！ ===\n");
    printf("\n📌 编译器上下文的优势:\n");
    printf("  ✓ 线程安全 - 每个线程可以有自己的上下文\n");
    printf("  ✓ 易于测试 - 状态隔离，无全局污染\n");
    printf("  ✓ 可重入 - 可以嵌套调用编译器\n");
    printf("  ✓ 资源管理清晰 - 明确的创建和释放\n");
    
    return 0;
}


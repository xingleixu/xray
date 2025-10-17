/*
** test_vm_context.c
** 测试VM上下文
*/

#include "xvm_context.h"
#include <stdio.h>
#include <assert.h>

int main(void) {
    printf("=== 测试VM上下文 ===\n\n");
    
    /* 测试1: 创建和释放上下文 */
    printf("测试1: 创建和释放上下文\n");
    VMContext *ctx = xr_vm_context_new();
    assert(ctx != NULL);
    assert(ctx->vm != NULL);
    assert(ctx->owns_vm == true);
    assert(ctx->total_instructions == 0);
    assert(ctx->total_calls == 0);
    printf("✓ VM上下文创建成功\n");
    printf("  - VM实例: %p\n", (void*)ctx->vm);
    printf("  - 拥有VM: %s\n", ctx->owns_vm ? "是" : "否");
    printf("\n");
    
    /* 测试2: VM初始化状态 */
    printf("测试2: VM初始化状态\n");
    assert(ctx->vm->stack_top == ctx->vm->stack);
    assert(ctx->vm->frame_count == 0);
    assert(ctx->vm->open_upvalues == NULL);
    assert(ctx->vm->global_count == 0);
    printf("✓ VM状态初始化正确\n");
    printf("  - 栈顶: %p\n", (void*)ctx->vm->stack_top);
    printf("  - 帧数: %d\n", ctx->vm->frame_count);
    printf("  - 全局变量数: %d\n", ctx->vm->global_count);
    printf("\n");
    
    /* 测试3: 栈操作 */
    printf("测试3: 栈操作\n");
    xr_vm_ctx_push(ctx, xr_int(42));
    xr_vm_ctx_push(ctx, xr_int(100));
    xr_vm_ctx_push(ctx, xr_int(200));
    
    assert(ctx->vm->stack_top - ctx->vm->stack == 3);
    printf("✓ 压入3个值，栈深度=%ld\n", ctx->vm->stack_top - ctx->vm->stack);
    
    XrValue v1 = xr_vm_ctx_peek(ctx, 0);  /* 栈顶 */
    XrValue v2 = xr_vm_ctx_peek(ctx, 1);
    XrValue v3 = xr_vm_ctx_peek(ctx, 2);
    
    assert(xr_toint(v1) == 200);
    assert(xr_toint(v2) == 100);
    assert(xr_toint(v3) == 42);
    printf("✓ peek(0)=%ld, peek(1)=%ld, peek(2)=%ld\n",
           xr_toint(v1), xr_toint(v2), xr_toint(v3));
    
    XrValue popped = xr_vm_ctx_pop(ctx);
    assert(xr_toint(popped) == 200);
    assert(ctx->vm->stack_top - ctx->vm->stack == 2);
    printf("✓ pop()=%ld, 栈深度=%ld\n",
           xr_toint(popped), ctx->vm->stack_top - ctx->vm->stack);
    printf("\n");
    
    /* 测试4: 全局变量操作 */
    printf("测试4: 全局变量操作\n");
    xr_vm_ctx_set_global(ctx, 0, xr_int(10));
    xr_vm_ctx_set_global(ctx, 1, xr_int(20));
    xr_vm_ctx_set_global(ctx, 5, xr_int(50));
    
    assert(ctx->vm->global_count == 6);  /* 索引5+1 */
    printf("✓ 设置全局变量，数量=%d\n", ctx->vm->global_count);
    
    XrValue g0 = xr_vm_ctx_get_global(ctx, 0);
    XrValue g1 = xr_vm_ctx_get_global(ctx, 1);
    XrValue g5 = xr_vm_ctx_get_global(ctx, 5);
    
    assert(xr_toint(g0) == 10);
    assert(xr_toint(g1) == 20);
    assert(xr_toint(g5) == 50);
    printf("✓ global[0]=%ld, global[1]=%ld, global[5]=%ld\n",
           xr_toint(g0), xr_toint(g1), xr_toint(g5));
    printf("\n");
    
    /* 测试5: 上下文重置 */
    printf("测试5: 上下文重置\n");
    xr_vm_context_reset(ctx);
    assert(ctx->vm->stack_top == ctx->vm->stack);
    assert(ctx->vm->frame_count == 0);
    assert(ctx->vm->global_count == 0);
    assert(ctx->total_instructions == 0);
    assert(ctx->total_calls == 0);
    printf("✓ 上下文重置成功\n\n");
    
    /* 测试6: 多个上下文并存 */
    printf("测试6: 多个上下文并存（验证隔离）\n");
    VMContext *ctx2 = xr_vm_context_new();
    assert(ctx2 != NULL);
    assert(ctx2 != ctx);
    assert(ctx2->vm != ctx->vm);  /* 不同的VM实例 */
    
    /* 在ctx2中操作，不影响ctx */
    xr_vm_ctx_push(ctx2, xr_int(999));
    xr_vm_ctx_set_global(ctx2, 0, xr_int(888));
    
    assert(ctx2->vm->stack_top - ctx2->vm->stack == 1);
    assert(ctx->vm->stack_top - ctx->vm->stack == 0);  /* ctx不受影响 */
    printf("✓ 多个上下文完全隔离\n");
    printf("  - ctx1栈深度=%ld, ctx2栈深度=%ld\n",
           ctx->vm->stack_top - ctx->vm->stack,
           ctx2->vm->stack_top - ctx2->vm->stack);
    printf("\n");
    
    /* 测试7: 统计信息 */
    printf("测试7: 统计信息\n");
    ctx->total_instructions = 1000;
    ctx->total_calls = 10;
    ctx->execution_time = 0.123;
    
    size_t inst_count, call_count;
    double exec_time;
    xr_vm_ctx_get_stats(ctx, &inst_count, &call_count, &exec_time);
    
    assert(inst_count == 1000);
    assert(call_count == 10);
    assert(exec_time == 0.123);
    printf("✓ 统计信息获取正确\n");
    printf("  - 指令数: %zu\n", inst_count);
    printf("  - 调用数: %zu\n", call_count);
    printf("  - 执行时间: %.3f秒\n", exec_time);
    printf("\n");
    
    /* 测试8: 调试功能 */
    printf("测试8: 调试功能\n");
    assert(ctx->vm->trace_execution == false);
    xr_vm_ctx_set_trace(ctx, true);
    assert(ctx->vm->trace_execution == true);
    printf("✓ 执行跟踪启用/禁用成功\n\n");
    
    /* 清理 */
    xr_vm_context_free(ctx);
    xr_vm_context_free(ctx2);
    
    printf("=== 所有测试通过！ ===\n");
    printf("\n📌 VM上下文的优势:\n");
    printf("  ✓ 线程安全 - 每个线程可以有独立的VM\n");
    printf("  ✓ 易于测试 - VM状态完全隔离\n");
    printf("  ✓ 资源管理清晰 - 明确的创建和释放\n");
    printf("  ✓ 统计和调试 - 内置性能分析和调试功能\n");
    printf("  ✓ 多VM实例 - 可以运行多个独立的脚本环境\n");
    
    return 0;
}


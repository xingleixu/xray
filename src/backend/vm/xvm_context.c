/*
** xvm_context.c
** Xray 虚拟机上下文实现
*/

#include "xvm_context.h"
#include "xmem.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ========== 上下文管理 ========== */

/*
** 创建VM上下文
*/
VMContext* xr_vm_context_new(void) {
    VMContext *ctx = (VMContext*)xmem_alloc(sizeof(VMContext));
    if (!ctx) {
        return NULL;
    }
    
    /* 创建新的VM实例 */
    ctx->vm = (VM*)xmem_alloc(sizeof(VM));
    if (!ctx->vm) {
        xmem_free(ctx);
        return NULL;
    }
    
    ctx->owns_vm = true;
    
    /* 初始化统计信息 */
    ctx->total_instructions = 0;
    ctx->total_calls = 0;
    ctx->execution_time = 0.0;
    
    /* 初始化配置 */
    ctx->enable_profiling = false;
    ctx->enable_strict_mode = false;
    
    /* 初始化VM */
    xr_vm_ctx_init(ctx);
    
    return ctx;
}

/*
** 创建VM上下文（包装现有VM）
*/
VMContext* xr_vm_context_wrap(VM *vm, bool take_ownership) {
    if (!vm) return NULL;
    
    VMContext *ctx = (VMContext*)xmem_alloc(sizeof(VMContext));
    if (!ctx) {
        return NULL;
    }
    
    ctx->vm = vm;
    ctx->owns_vm = take_ownership;
    
    /* 初始化统计信息 */
    ctx->total_instructions = 0;
    ctx->total_calls = 0;
    ctx->execution_time = 0.0;
    
    /* 初始化配置 */
    ctx->enable_profiling = false;
    ctx->enable_strict_mode = false;
    
    return ctx;
}

/*
** 释放VM上下文
*/
void xr_vm_context_free(VMContext *ctx) {
    if (!ctx) return;
    
    /* 如果拥有VM，则释放VM */
    if (ctx->owns_vm && ctx->vm) {
        /* 注意：这里应该调用xr_bc_vm_free，但它使用全局vm
         * 实际使用时需要修改xr_bc_vm_free接受VM*参数
         * 现在先简单释放内存
         */
        xmem_free(ctx->vm);
    }
    
    /* 释放上下文本身 */
    xmem_free(ctx);
}

/*
** 重置VM上下文
*/
void xr_vm_context_reset(VMContext *ctx) {
    if (!ctx || !ctx->vm) return;
    
    /* 重置统计信息 */
    ctx->total_instructions = 0;
    ctx->total_calls = 0;
    ctx->execution_time = 0.0;
    
    /* 重置VM状态 */
    ctx->vm->stack_top = ctx->vm->stack;
    ctx->vm->frame_count = 0;
    ctx->vm->open_upvalues = NULL;
    ctx->vm->global_count = 0;
}

/* ========== VM操作 ========== */

/*
** 初始化VM
*/
void xr_vm_ctx_init(VMContext *ctx) {
    if (!ctx || !ctx->vm) return;
    
    VM *vm = ctx->vm;
    
    /* 初始化栈 */
    vm->stack_top = vm->stack;
    
    /* 初始化调用帧 */
    vm->frame_count = 0;
    
    /* 初始化upvalue链表 */
    vm->open_upvalues = NULL;
    
    /* 初始化全局变量 */
    vm->global_count = 0;
    for (int i = 0; i < 256; i++) {
        vm->globals_array[i] = xr_null();
    }
    
    /* 初始化字符串驻留表 */
    vm->strings = NULL;  /* 延迟初始化 */
    
    /* 初始化GC */
    vm->objects = NULL;
    vm->bytes_allocated = 0;
    vm->next_gc = 1024 * 1024;  /* 1MB */
    
    /* 初始化调试选项 */
    vm->trace_execution = false;
}

/*
** 执行源代码（使用上下文）
*/
InterpretResult xr_vm_ctx_interpret(VMContext *ctx, const char *source) {
    if (!ctx || !ctx->vm || !source) {
        return INTERPRET_RUNTIME_ERROR;
    }
    
    /* VM上下文执行：框架已完成，完全迁移在Week 3进行 */
    
    /* 记录开始时间 */
    clock_t start = clock();
    
    /* 执行字节码 */
    InterpretResult result = xr_bc_interpret(source);
    
    /* 记录执行时间 */
    clock_t end = clock();
    ctx->execution_time += (double)(end - start) / CLOCKS_PER_SEC;
    
    return result;
}

/*
** 执行函数原型（使用上下文）
*/
InterpretResult xr_vm_ctx_interpret_proto(VMContext *ctx, Proto *proto) {
    if (!ctx || !ctx->vm || !proto) {
        return INTERPRET_RUNTIME_ERROR;
    }
    
    /* 记录开始时间 */
    clock_t start = clock();
    
    /* 调用原有函数（使用ctx中的vm）*/
    InterpretResult result = xr_bc_interpret_proto(ctx->vm, proto);
    
    /* 记录执行时间 */
    clock_t end = clock();
    ctx->execution_time += (double)(end - start) / CLOCKS_PER_SEC;
    ctx->total_calls++;
    
    return result;
}

/* ========== 栈操作 ========== */

/*
** 压入值到栈顶
*/
void xr_vm_ctx_push(VMContext *ctx, XrValue value) {
    if (!ctx || !ctx->vm) return;
    *ctx->vm->stack_top = value;
    ctx->vm->stack_top++;
}

/*
** 从栈顶弹出值
*/
XrValue xr_vm_ctx_pop(VMContext *ctx) {
    if (!ctx || !ctx->vm) return xr_null();
    ctx->vm->stack_top--;
    return *ctx->vm->stack_top;
}

/*
** 获取栈顶值
*/
XrValue xr_vm_ctx_peek(VMContext *ctx, int distance) {
    if (!ctx || !ctx->vm) return xr_null();
    return ctx->vm->stack_top[-1 - distance];
}

/* ========== 全局变量操作 ========== */

/*
** 设置全局变量
*/
void xr_vm_ctx_set_global(VMContext *ctx, int index, XrValue value) {
    if (!ctx || !ctx->vm) return;
    if (index < 0 || index >= 256) return;
    ctx->vm->globals_array[index] = value;
    if (index >= ctx->vm->global_count) {
        ctx->vm->global_count = index + 1;
    }
}

/*
** 获取全局变量
*/
XrValue xr_vm_ctx_get_global(VMContext *ctx, int index) {
    if (!ctx || !ctx->vm) return xr_null();
    if (index < 0 || index >= 256) return xr_null();
    return ctx->vm->globals_array[index];
}

/* ========== 统计信息 ========== */

/*
** 获取执行统计
*/
void xr_vm_ctx_get_stats(VMContext *ctx,
                        size_t *out_instructions,
                        size_t *out_calls,
                        double *out_time) {
    if (!ctx) return;
    
    if (out_instructions) *out_instructions = ctx->total_instructions;
    if (out_calls) *out_calls = ctx->total_calls;
    if (out_time) *out_time = ctx->execution_time;
}

/*
** 打印统计信息
*/
void xr_vm_ctx_print_stats(VMContext *ctx) {
    if (!ctx) return;
    
    printf("=== VM Statistics ===\n");
    printf("Instructions executed: %zu\n", ctx->total_instructions);
    printf("Function calls: %zu\n", ctx->total_calls);
    printf("Execution time: %.6f seconds\n", ctx->execution_time);
    
    if (ctx->vm) {
        printf("Global variables: %d\n", ctx->vm->global_count);
        printf("Bytes allocated: %zu\n", ctx->vm->bytes_allocated);
        printf("Stack depth: %ld\n", ctx->vm->stack_top - ctx->vm->stack);
        printf("Call depth: %d\n", ctx->vm->frame_count);
    }
}

/* ========== 调试功能 ========== */

/*
** 启用/禁用执行跟踪
*/
void xr_vm_ctx_set_trace(VMContext *ctx, bool enable) {
    if (!ctx || !ctx->vm) return;
    ctx->vm->trace_execution = enable;
}

/*
** 打印当前栈
*/
void xr_vm_ctx_print_stack(VMContext *ctx) {
    if (!ctx || !ctx->vm) return;
    
    VM *vm = ctx->vm;
    printf("=== Stack ===\n");
    for (XrValue *slot = vm->stack; slot < vm->stack_top; slot++) {
        printf("[ ");
        /* 值打印：可实现xr_value_print统一打印函数 */
        printf("%ld", slot - vm->stack);
        printf(" ]\n");
    }
}

/*
** 打印调用栈
*/
void xr_vm_ctx_print_callstack(VMContext *ctx) {
    if (!ctx || !ctx->vm) return;
    
    VM *vm = ctx->vm;
    printf("=== Call Stack ===\n");
    for (int i = 0; i < vm->frame_count; i++) {
        BcCallFrame *frame = &vm->frames[i];
        Proto *proto = frame->closure->proto;
        printf("Frame %d: ", i);
        if (proto->name) {
            printf("%s", proto->name);
        } else {
            printf("<script>");
        }
        printf("\n");
    }
}


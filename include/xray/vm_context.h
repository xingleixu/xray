/*
** xvm_context.h
** Xray 虚拟机上下文 - 消除全局VM实例
** 
** 职责：
**   - 封装VM的全局状态
**   - 支持多VM实例（多线程）
**   - 便于测试和隔离
** 
** v0.13.7: 新增VM上下文
*/

#ifndef xvm_context_h
#define xvm_context_h

#include "xvm.h"

/* ========== VM上下文 ========== */

/*
** VM上下文
** 包装VM结构，提供额外的管理功能
*/
typedef struct {
    VM *vm;                     /* VM实例指针 */
    bool owns_vm;               /* 是否拥有VM（负责释放）*/
    
    /* 统计信息 */
    size_t total_instructions;  /* 执行的指令总数 */
    size_t total_calls;         /* 函数调用总数 */
    double execution_time;      /* 总执行时间（秒）*/
    
    /* 配置选项 */
    bool enable_profiling;      /* 是否启用性能分析 */
    bool enable_strict_mode;    /* 是否启用严格模式 */
} VMContext;

/* ========== 上下文管理 ========== */

/*
** 创建VM上下文
** @return 新的VM上下文，失败返回NULL
*/
VMContext* xr_vm_context_new(void);

/*
** 创建VM上下文（使用现有VM）
** @param vm 现有的VM实例
** @param take_ownership 是否接管VM的所有权
** @return 新的VM上下文，失败返回NULL
*/
VMContext* xr_vm_context_wrap(VM *vm, bool take_ownership);

/*
** 释放VM上下文
** @param ctx 要释放的上下文
*/
void xr_vm_context_free(VMContext *ctx);

/*
** 重置VM上下文（清空状态但不释放）
** @param ctx 要重置的上下文
*/
void xr_vm_context_reset(VMContext *ctx);

/* ========== VM操作（通过上下文）========== */

/*
** 初始化VM
** @param ctx VM上下文
*/
void xr_vm_ctx_init(VMContext *ctx);

/*
** 执行源代码
** @param ctx VM上下文
** @param source 源代码字符串
** @return 执行结果
*/
InterpretResult xr_vm_ctx_interpret(VMContext *ctx, const char *source);

/*
** 执行函数原型
** @param ctx VM上下文
** @param proto 函数原型
** @return 执行结果
*/
InterpretResult xr_vm_ctx_interpret_proto(VMContext *ctx, Proto *proto);

/* ========== 栈操作 ========== */

/*
** 压入值到栈顶
** @param ctx VM上下文
** @param value 要压入的值
*/
void xr_vm_ctx_push(VMContext *ctx, XrValue value);

/*
** 从栈顶弹出值
** @param ctx VM上下文
** @return 弹出的值
*/
XrValue xr_vm_ctx_pop(VMContext *ctx);

/*
** 获取栈顶值（不弹出）
** @param ctx VM上下文
** @param distance 距离栈顶的距离（0=栈顶）
** @return 栈上的值
*/
XrValue xr_vm_ctx_peek(VMContext *ctx, int distance);

/* ========== 全局变量操作 ========== */

/*
** 设置全局变量
** @param ctx VM上下文
** @param index 全局变量索引
** @param value 值
*/
void xr_vm_ctx_set_global(VMContext *ctx, int index, XrValue value);

/*
** 获取全局变量
** @param ctx VM上下文
** @param index 全局变量索引
** @return 全局变量的值
*/
XrValue xr_vm_ctx_get_global(VMContext *ctx, int index);

/* ========== 统计信息 ========== */

/*
** 获取执行统计
** @param ctx VM上下文
** @param out_instructions 输出：执行的指令数
** @param out_calls 输出：函数调用数
** @param out_time 输出：执行时间（秒）
*/
void xr_vm_ctx_get_stats(VMContext *ctx, 
                        size_t *out_instructions,
                        size_t *out_calls,
                        double *out_time);

/*
** 打印统计信息
** @param ctx VM上下文
*/
void xr_vm_ctx_print_stats(VMContext *ctx);

/* ========== 调试功能 ========== */

/*
** 启用/禁用执行跟踪
** @param ctx VM上下文
** @param enable 是否启用
*/
void xr_vm_ctx_set_trace(VMContext *ctx, bool enable);

/*
** 打印当前栈
** @param ctx VM上下文
*/
void xr_vm_ctx_print_stack(VMContext *ctx);

/*
** 打印调用栈
** @param ctx VM上下文
*/
void xr_vm_ctx_print_callstack(VMContext *ctx);

#endif /* xvm_context_h */


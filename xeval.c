/*
** xeval.c
** Xray 表达式求值器实现
** 使用访问者模式遍历 AST 并计算结果
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include "xeval.h"
#include "xstate.h"
#include "xarray.h"  /* 第九阶段新增：数组支持 */

/* 最大调用深度（防止栈溢出） */
#define MAX_CALL_DEPTH 1000

/* 内部求值函数前向声明 */
static XrValue xr_eval_internal(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret);
static XrValue xr_eval_literal(XrayState *X, AstNode *node);
static XrValue xr_eval_binary(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret);
static XrValue xr_eval_unary(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret);
static XrValue xr_eval_grouping(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret);
static XrValue xr_eval_expr_stmt(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret);
static XrValue xr_eval_print_stmt(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret);
static XrValue xr_eval_block_internal(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret);
static XrValue xr_eval_program(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret);
static XrValue xr_eval_logical_and(XrayState *X, AstNode *left_node, AstNode *right_node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret);
static XrValue xr_eval_logical_or(XrayState *X, AstNode *left_node, AstNode *right_node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret);

/* ========== 主要求值函数 ========== */

/*
** 主求值函数 - 创建符号表并开始求值
*/
XrValue xr_eval(XrayState *X, AstNode *node) {
    /* 创建符号表 */
    XSymbolTable *symbols = xsymboltable_new();
    if (!symbols) {
        xr_runtime_error(X, 0, "内存分配失败");
        return xr_null();  /* 新API */
    }
    
    /* 初始化循环控制和返回控制 */
    LoopControl loop = {LOOP_NONE, 0};
    ReturnControl ret = {0, {XR_TNULL}};
    ret.return_value = xr_null();  /* 新API */
    
    /* 调用内部求值函数 */
    XrValue result = xr_eval_internal(X, node, symbols, &loop, &ret);
    
    /* 释放符号表 */
    xsymboltable_free(symbols);
    
    return result;
}

/*
** 使用外部符号表的求值函数（用于 REPL）
** 允许跨多次调用保持变量状态
*/
XrValue xr_eval_with_symbols(XrayState *X, AstNode *node, XSymbolTable *symbols) {
    if (!symbols) {
        xr_runtime_error(X, 0, "符号表为空");
        return xr_null();  /* 新API */
    }
    
    /* 初始化循环控制和返回控制 */
    LoopControl loop = {LOOP_NONE, 0};
    ReturnControl ret = {0, {XR_TNULL}};
    ret.return_value = xr_null();  /* 新API */
    
    return xr_eval_internal(X, node, symbols, &loop, &ret);
}

/*
** 内部求值函数 - AST 访问者模式的核心
** 根据节点类型分发到相应的处理函数
*/
static XrValue xr_eval_internal(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    if (node == NULL) {
        return xr_null();  /* 新API */
    }
    
    switch (node->type) {
        /* 字面量节点 */
        case AST_LITERAL_INT:
        case AST_LITERAL_FLOAT:
        case AST_LITERAL_STRING:
        case AST_LITERAL_NULL:
        case AST_LITERAL_TRUE:
        case AST_LITERAL_FALSE:
            return xr_eval_literal(X, node);
        
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
            return xr_eval_binary(X, node, symbols, loop, ret);
        
        /* 一元运算节点 */
        case AST_UNARY_NEG:
        case AST_UNARY_NOT:
            return xr_eval_unary(X, node, symbols, loop, ret);
        
        /* 分组节点 */
        case AST_GROUPING:
            return xr_eval_grouping(X, node, symbols, loop, ret);
        
        /* 语句节点 */
        case AST_EXPR_STMT:
            return xr_eval_expr_stmt(X, node, symbols, loop, ret);
        
        case AST_PRINT_STMT:
            return xr_eval_print_stmt(X, node, symbols, loop, ret);
        
        case AST_BLOCK:
            return xr_eval_block_internal(X, node, symbols, loop, ret);
        
        /* 变量相关节点 */
        case AST_VAR_DECL:
        case AST_CONST_DECL:
            return xr_eval_var_decl(X, node, symbols, loop, ret);
        
        case AST_VARIABLE:
            return xr_eval_variable(X, node, symbols);
        
        case AST_ASSIGNMENT:
            return xr_eval_assignment(X, node, symbols, loop, ret);
        
        /* 控制流节点 */
        case AST_IF_STMT:
            return xr_eval_if_stmt(X, node, symbols, loop, ret);
        
        case AST_WHILE_STMT:
            return xr_eval_while_stmt(X, node, symbols, loop, ret);
        
        case AST_FOR_STMT:
            return xr_eval_for_stmt(X, node, symbols, loop, ret);
        
        case AST_BREAK_STMT:
            return xr_eval_break_stmt(X, node, symbols, loop);
        
        case AST_CONTINUE_STMT:
            return xr_eval_continue_stmt(X, node, symbols, loop);
        
        /* 函数相关节点 */
        case AST_FUNCTION_DECL:
            return xr_eval_function_decl(X, node, symbols);
        
        case AST_FUNCTION_EXPR:
            return xr_eval_function_expr(X, node, symbols);
        
        case AST_CALL_EXPR:
            return xr_eval_call_expr(X, node, symbols, loop, ret);
        
        case AST_RETURN_STMT:
            return xr_eval_return_stmt(X, node, symbols, loop, ret);
        
        /* 数组相关节点（第九阶段新增） */
        case AST_ARRAY_LITERAL:
            return xr_eval_array_literal(X, node, symbols, loop, ret);
        
        case AST_INDEX_GET:
            return xr_eval_index_get(X, node, symbols, loop, ret);
        
        case AST_INDEX_SET:
            return xr_eval_index_set(X, node, symbols, loop, ret);
        
        case AST_MEMBER_ACCESS:
            return xr_eval_member_access(X, node, symbols, loop, ret);
        
        case AST_PROGRAM:
            return xr_eval_program(X, node, symbols, loop, ret);
        
        default:
            xr_runtime_error(X, node->line, "未知的 AST 节点类型: %d", node->type);
            
            return xr_null();  /* 新API */

    }
}

/* ========== 具体节点求值函数 ========== */

/*
** 字面量求值 - 直接返回存储的值
*/
static XrValue xr_eval_literal(XrayState *X, AstNode *node) {
    (void)X;  /* 未使用，避免警告 */
    return node->as.literal.value;
}

/*
** 二元运算求值
*/
static XrValue xr_eval_binary(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    /* 逻辑运算需要短路求值，特殊处理 */
    if (node->type == AST_BINARY_AND) {
        return xr_eval_logical_and(X, node->as.binary.left, node->as.binary.right, symbols, loop, ret);
    }
    if (node->type == AST_BINARY_OR) {
        return xr_eval_logical_or(X, node->as.binary.left, node->as.binary.right, symbols, loop, ret);
    }
    
    /* 其他运算：先求值两个操作数 */
    XrValue left = xr_eval_internal(X, node->as.binary.left, symbols, loop, ret);
    XrValue right = xr_eval_internal(X, node->as.binary.right, symbols, loop, ret);
    
    /* 根据运算符类型执行计算 */
    switch (node->type) {
        case AST_BINARY_ADD:    return xr_eval_add(X, left, right);
        case AST_BINARY_SUB:    return xr_eval_subtract(X, left, right);
        case AST_BINARY_MUL:    return xr_eval_multiply(X, left, right);
        case AST_BINARY_DIV:    return xr_eval_divide(X, left, right);
        case AST_BINARY_MOD:    return xr_eval_modulo(X, left, right);
        case AST_BINARY_EQ:     return xr_eval_equal(X, left, right);
        case AST_BINARY_NE:     return xr_eval_not_equal(X, left, right);
        case AST_BINARY_LT:     return xr_eval_less(X, left, right);
        case AST_BINARY_LE:     return xr_eval_less_equal(X, left, right);
        case AST_BINARY_GT:     return xr_eval_greater(X, left, right);
        case AST_BINARY_GE:     return xr_eval_greater_equal(X, left, right);
        
        default:
            xr_runtime_error(X, node->line, "未知的二元运算符");
            
            return xr_null();  /* 新API */

    }
}

/*
** 一元运算求值
*/
static XrValue xr_eval_unary(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    XrValue operand = xr_eval_internal(X, node->as.unary.operand, symbols, loop, ret);
    
    switch (node->type) {
        case AST_UNARY_NEG:
            return xr_eval_negate(X, operand);
        case AST_UNARY_NOT:
            return xr_eval_logical_not(X, operand);
        
        default:
            xr_runtime_error(X, node->line, "未知的一元运算符");
            
            return xr_null();  /* 新API */

    }
}

/*
** 分组表达式求值（括号）
*/
static XrValue xr_eval_grouping(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    return xr_eval_internal(X, node->as.grouping, symbols, loop, ret);
}

/*
** 表达式语句求值
*/
static XrValue xr_eval_expr_stmt(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    return xr_eval_internal(X, node->as.expr_stmt, symbols, loop, ret);
}

/*
** print 语句求值
*/
static XrValue xr_eval_print_stmt(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    XrValue value = xr_eval_internal(X, node->as.print_stmt.expr, symbols, loop, ret);
    
    /* 打印值 */
    const char *str = xr_value_to_string(X, value);
    printf("%s\n", str);
    
    /* 返回 null */
    
        return xr_null();  /* 新API */

}

/*
** 程序求值（执行多个语句）
*/
static XrValue xr_eval_program(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    XrValue last_value = xr_null();  /* 新API */

    
    /* 执行所有语句，返回最后一个语句的值 */
    for (int i = 0; i < node->as.program.count; i++) {
        last_value = xr_eval_internal(X, node->as.program.statements[i], symbols, loop, ret);
        
        /* 检查是否已返回 */
        if (ret->has_returned) {
            return ret->return_value;
        }
    }
    
    return last_value;
}

/* ========== 算术运算实现 ========== */

/*
** 加法运算
** 支持：数字 + 数字、字符串 + 字符串
*/
XrValue xr_eval_add(XrayState *X, XrValue left, XrValue right) {
    XrValue result = xr_null();  /* 新API - 默认初始化为null */
    
    /* 字符串拼接 */
    if (xr_isstring(left) && xr_isstring(right)) {
        /* 简单实现：字符串拼接 */
        const char *left_str = (const char *)xr_toobj(left);
        const char *right_str = (const char *)xr_toobj(right);
        
        int left_len = strlen(left_str);
        int right_len = strlen(right_str);
        char *new_str = (char *)malloc(left_len + right_len + 1);
        
        strcpy(new_str, left_str);
        strcat(new_str, right_str);
        
        result.type = XR_TSTRING;
        result.as.obj = new_str;
        return result;
    }
    
    /* 数字加法 */
    if (xr_is_number(left) && xr_is_number(right)) {
        xr_Number left_num = xr_to_number(X, left);
        xr_Number right_num = xr_to_number(X, right);
        
        /* 如果两个都是整数，结果也是整数 */
        if (xr_isint(left) && xr_isint(right)) {
            result = xr_int((xr_Integer)(left_num + right_num));  /* 新API */
        } else {
            result = xr_float(left_num + right_num);  /* 新API */
        }
        return result;
    }
    
    xr_runtime_error(X, 0, "加法运算的操作数必须都是数字或都是字符串");

    return result;
}

/*
** 减法运算
*/
XrValue xr_eval_subtract(XrayState *X, XrValue left, XrValue right) {
    XrValue result = xr_null();  /* 新API - 默认初始化为null */
    
    if (!xr_is_number(left) || !xr_is_number(right)) {
        xr_runtime_error(X, 0, "减法运算的操作数必须是数字");

        return result;
    }
    
    xr_Number left_num = xr_to_number(X, left);
    xr_Number right_num = xr_to_number(X, right);
    
    if (xr_isint(left) && xr_isint(right)) {
        result = xr_int((xr_Integer)(left_num - right_num));  /* 新API */
    } else {
        result = xr_float(left_num - right_num);  /* 新API */
    }
    
    return result;
}

/*
** 乘法运算
*/
XrValue xr_eval_multiply(XrayState *X, XrValue left, XrValue right) {
    XrValue result = xr_null();  /* 新API - 默认初始化为null */
    
    if (!xr_is_number(left) || !xr_is_number(right)) {
        xr_runtime_error(X, 0, "乘法运算的操作数必须是数字");

        return result;
    }
    
    xr_Number left_num = xr_to_number(X, left);
    xr_Number right_num = xr_to_number(X, right);
    
    if (xr_isint(left) && xr_isint(right)) {
        result = xr_int((xr_Integer)(left_num * right_num));  /* 新API */
    } else {
        result = xr_float(left_num * right_num);  /* 新API */
    }
    
    return result;
}

/*
** 除法运算
*/
XrValue xr_eval_divide(XrayState *X, XrValue left, XrValue right) {
    XrValue result = xr_null();  /* 新API - 默认初始化为null */
    
    if (!xr_is_number(left) || !xr_is_number(right)) {
        xr_runtime_error(X, 0, "除法运算的操作数必须是数字");

        return result;
    }
    
    xr_Number left_num = xr_to_number(X, left);
    xr_Number right_num = xr_to_number(X, right);
    
    if (right_num == 0.0) {
        xr_runtime_error(X, 0, "除零错误");

        return result;
    }
    
    /* 除法结果总是浮点数 */
    result = xr_float(left_num / right_num);  /* 新API */
    return result;
}

/*
** 取模运算
*/
XrValue xr_eval_modulo(XrayState *X, XrValue left, XrValue right) {
    XrValue result = xr_null();  /* 新API - 默认初始化为null */
    
    if (!xr_is_number(left) || !xr_is_number(right)) {
        xr_runtime_error(X, 0, "取模运算的操作数必须是数字");

        return result;
    }
    
    xr_Number left_num = xr_to_number(X, left);
    xr_Number right_num = xr_to_number(X, right);
    
    if (right_num == 0.0) {
        xr_runtime_error(X, 0, "取模运算的除数不能为零");

        return result;
    }
    
    if (xr_isint(left) && xr_isint(right)) {
        result = xr_int(xr_toint(left) % xr_toint(right));  /* 新API */
    } else {
        result = xr_float(fmod(left_num, right_num));  /* 新API */
    }
    
    return result;
}

/* ========== 比较运算实现 ========== */

/*
** 相等比较
*/
XrValue xr_eval_equal(XrayState *X, XrValue left, XrValue right) {
    XrValue result = xr_null();  /* 新API - 默认初始化为null */
    result = xr_bool(xr_values_equal(left, right));  /* 新API */
    return result;
}

/*
** 不等比较
*/
XrValue xr_eval_not_equal(XrayState *X, XrValue left, XrValue right) {
    XrValue result = xr_null();  /* 新API - 默认初始化为null */
    result = xr_bool(!xr_values_equal(left, right));  /* 新API */
    return result;
}

/*
** 小于比较
*/
XrValue xr_eval_less(XrayState *X, XrValue left, XrValue right) {
    XrValue result = xr_null();  /* 新API - 默认初始化为null */
    
    if (!xr_is_number(left) || !xr_is_number(right)) {
        xr_runtime_error(X, 0, "比较运算的操作数必须是数字");

        return result;
    }
    
    xr_Number left_num = xr_to_number(X, left);
    xr_Number right_num = xr_to_number(X, right);
    
    result = xr_bool(left_num < right_num);  /* 新API */
    return result;
}

/*
** 小于等于比较
*/
XrValue xr_eval_less_equal(XrayState *X, XrValue left, XrValue right) {
    XrValue result = xr_null();  /* 新API - 默认初始化为null */
    
    if (!xr_is_number(left) || !xr_is_number(right)) {
        xr_runtime_error(X, 0, "比较运算的操作数必须是数字");

        return result;
    }
    
    xr_Number left_num = xr_to_number(X, left);
    xr_Number right_num = xr_to_number(X, right);
    
    result = xr_bool(left_num <= right_num);  /* 新API */
    return result;
}

/*
** 大于比较
*/
XrValue xr_eval_greater(XrayState *X, XrValue left, XrValue right) {
    XrValue result = xr_null();  /* 新API - 默认初始化为null */
    
    if (!xr_is_number(left) || !xr_is_number(right)) {
        xr_runtime_error(X, 0, "比较运算的操作数必须是数字");

        return result;
    }
    
    xr_Number left_num = xr_to_number(X, left);
    xr_Number right_num = xr_to_number(X, right);
    
    result = xr_bool(left_num > right_num);  /* 新API */
    return result;
}

/*
** 大于等于比较
*/
XrValue xr_eval_greater_equal(XrayState *X, XrValue left, XrValue right) {
    XrValue result = xr_null();  /* 新API - 默认初始化为null */
    
    if (!xr_is_number(left) || !xr_is_number(right)) {
        xr_runtime_error(X, 0, "比较运算的操作数必须是数字");

        return result;
    }
    
    xr_Number left_num = xr_to_number(X, left);
    xr_Number right_num = xr_to_number(X, right);
    
    result = xr_bool(left_num >= right_num);  /* 新API */
    return result;
}

/* ========== 逻辑运算实现（支持短路求值）========== */

/*
** 逻辑与（短路求值）
*/
static XrValue xr_eval_logical_and(XrayState *X, AstNode *left_node, AstNode *right_node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    XrValue left = xr_eval_internal(X, left_node, symbols, loop, ret);
    
    /* 如果左边为假，直接返回左边的值（短路） */
    if (!xr_is_truthy(left)) {
        return left;
    }
    
    /* 否则返回右边的值 */
    return xr_eval_internal(X, right_node, symbols, loop, ret);
}

/*
** 逻辑或（短路求值）
*/
static XrValue xr_eval_logical_or(XrayState *X, AstNode *left_node, AstNode *right_node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    XrValue left = xr_eval_internal(X, left_node, symbols, loop, ret);
    
    /* 如果左边为真，直接返回左边的值（短路） */
    if (xr_is_truthy(left)) {
        return left;
    }
    
    /* 否则返回右边的值 */
    return xr_eval_internal(X, right_node, symbols, loop, ret);
}

/*
** 逻辑非
*/
XrValue xr_eval_logical_not(XrayState *X, XrValue operand) {
    XrValue result = xr_null();  /* 新API - 默认初始化为null */
    result = xr_bool(!xr_is_truthy(operand));  /* 新API */
    return result;
}

/*
** 取负运算
*/
XrValue xr_eval_negate(XrayState *X, XrValue operand) {
    XrValue result = xr_null();  /* 新API - 默认初始化为null */
    
    if (!xr_is_number(operand)) {
        xr_runtime_error(X, 0, "取负运算的操作数必须是数字");

        return result;
    }
    
    if (xr_isint(operand)) {
        result = xr_int(-xr_toint(operand));  /* 新API */
    } else {
        result = xr_float(-xr_tofloat(operand));  /* 新API */
    }
    
    return result;
}

/* ========== 辅助函数实现 ========== */

/*
** 检查值是否为数字
*/
int xr_is_number(XrValue value) {
    return xr_isint(value) || xr_isfloat(value);
}

/*
** 检查值的真假性
** 规则：null 和 false 为假，其他都为真
*/
int xr_is_truthy(XrValue value) {
    if (xr_isnull(value)) return 0;
    if (xr_isbool(value)) return xr_tobool(value);
    return 1;  /* 其他值都为真 */
}

/*
** 将值转换为数字
*/
xr_Number xr_to_number(XrayState *X, XrValue value) {
    if (xr_isint(value)) {
        return (xr_Number)xr_toint(value);
    } else if (xr_isfloat(value)) {
        return xr_tofloat(value);
    } else {
        xr_runtime_error(X, 0, "无法将非数字值转换为数字");
        return 0.0;
    }
}

/*
** 值相等比较
*/
int xr_values_equal(XrValue a, XrValue b) {
    /* 类型不同，不相等 */
    if (a.type != b.type) {
        /* 特殊情况：整数和浮点数可以比较 */
        if ((xr_isint(a) && xr_isfloat(b)) || (xr_isfloat(a) && xr_isint(b))) {
            xr_Number a_num = xr_isint(a) ? (xr_Number)xr_toint(a) : xr_tofloat(a);
            xr_Number b_num = xr_isint(b) ? (xr_Number)xr_toint(b) : xr_tofloat(b);
            return a_num == b_num;
        }
        return 0;
    }
    
    /* 根据类型比较 */
    switch (a.type) {
        case XR_TNULL:   return 1;  /* null == null */
        case XR_TBOOL:   return xr_tobool(a) == xr_tobool(b);
        case XR_TINT:    return xr_toint(a) == xr_toint(b);
        case XR_TFLOAT:  return xr_tofloat(a) == xr_tofloat(b);
        case XR_TSTRING:
            return strcmp((const char *)xr_toobj(a), (const char *)xr_toobj(b)) == 0;
        default:         return 0;
    }
}

/*
** 将值转换为字符串表示
*/
const char *xr_value_to_string(XrayState *X, XrValue value) {
    static char buffer[64];  /* 简单实现，使用静态缓冲区 */
    
    switch (value.type) {
        case XR_TNULL:
            return "null";
        case XR_TBOOL:
            return xr_tobool(value) ? "true" : "false";
        case XR_TINT:
            snprintf(buffer, sizeof(buffer), "%lld", (long long)xr_toint(value));
            return buffer;
        case XR_TFLOAT:
            snprintf(buffer, sizeof(buffer), "%g", xr_tofloat(value));
            return buffer;
        case XR_TSTRING:
            return (const char *)xr_toobj(value);
        default:
            return "<unknown>";
    }
}

/* ========== 错误处理 ========== */

/*
** 运行时错误报告
*/
void xr_runtime_error(XrayState *X, int line, const char *format, ...) {
    fprintf(stderr, "运行时错误");
    if (line > 0) {
        fprintf(stderr, "[行 %d]", line);
    }
    fprintf(stderr, ": ");
    
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    
    fprintf(stderr, "\n");
}

/* ========== 变量相关求值函数 ========== */

/*
** 变量声明求值：let x = 10 或 const PI = 3.14
*/
XrValue xr_eval_var_decl(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    const char *name = node->as.var_decl.name;
    bool is_const = node->as.var_decl.is_const;
    
    /* 计算初始化表达式 */
    XrValue init_value;
    if (node->as.var_decl.initializer != NULL) {
        init_value = xr_eval_internal(X, node->as.var_decl.initializer, symbols, loop, ret);
    } else {
        /* 无初始化表达式，设为 null */
        init_value = xr_null();  /* 新API */
    }
    
    /* 在当前作用域定义变量 */
    if (!xsymboltable_define(symbols, name, init_value, is_const)) {
        xr_runtime_error(X, node->line, "变量 '%s' 已定义", name);
    }
    
    /* 返回 null */
    
        return xr_null();  /* 新API */

}

/*
** 变量引用求值：x
*/
XrValue xr_eval_variable(XrayState *X, AstNode *node, XSymbolTable *symbols) {
    const char *name = node->as.variable.name;
    
    /* 查找变量 */
    XrValue value;
    if (!xsymboltable_get(symbols, name, &value)) {
        xr_runtime_error(X, node->line, "未定义的变量 '%s'", name);
        return xr_null();  /* 新API */
    }
    
    return value;
}

/*
** 赋值求值：x = 10
*/
XrValue xr_eval_assignment(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    const char *name = node->as.assignment.name;
    
    /* 计算赋值表达式 */
    XrValue value = xr_eval_internal(X, node->as.assignment.value, symbols, loop, ret);
    
    /* 赋值 */
    if (!xsymboltable_assign(symbols, name, value)) {
        /* 检查是否是常量还是未定义 */
        XVariable *var = xsymboltable_resolve(symbols, name);
        if (var == NULL) {
            xr_runtime_error(X, node->line, "未定义的变量 '%s'", name);
        } else if (var->is_const) {
            xr_runtime_error(X, node->line, "不能修改常量 '%s'", name);
        }
        return xr_null();  /* 新API */
    }
    
    /* 赋值表达式返回赋的值 */
    return value;
}

/*
** 代码块求值：{ ... }
** 注意：block不需要loop参数，因为它内部会通过xr_eval_internal传递
** 但为了调用xr_eval_internal，我们需要创建一个临时的loop或接受外部的loop
** 这里我们通过在switch中直接调用内部实现来处理
*/
static XrValue xr_eval_block_internal(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    /* 进入新作用域 */
    xsymboltable_begin_scope(symbols);
    
    XrValue last_value = xr_null();  /* 新API */

    
    /* 执行代码块中的所有语句 */
    for (int i = 0; i < node->as.block.count; i++) {
        last_value = xr_eval_internal(X, node->as.block.statements[i], symbols, loop, ret);
        
        /* 如果遇到 return，停止执行 */
        if (ret && ret->has_returned) {
            break;
        }
        
        /* 如果遇到break/continue，停止执行剩余语句 */
        if (loop && (loop->state == LOOP_BREAK || loop->state == LOOP_CONTINUE)) {
            break;
        }
    }
    
    /* 退出作用域 */
    xsymboltable_end_scope(symbols);
    
    return last_value;
}

XrValue xr_eval_block(XrayState *X, AstNode *node, XSymbolTable *symbols) {
    LoopControl loop = {LOOP_NONE, 0};
    ReturnControl ret = {0, {XR_TNULL}};
    ret.return_value = xr_null();  /* 新API */
    return xr_eval_block_internal(X, node, symbols, &loop, &ret);
}

/* ========== 控制流语句求值 ========== */

/*
** if 语句求值
*/
XrValue xr_eval_if_stmt(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    /* 求值条件 */
    XrValue condition = xr_eval_internal(X, node->as.if_stmt.condition, symbols, loop, ret);
    
    /* 根据条件执行相应分支 */
    if (xr_is_truthy(condition)) {
        return xr_eval_block_internal(X, node->as.if_stmt.then_branch, symbols, loop, ret);
    } else if (node->as.if_stmt.else_branch != NULL) {
        /* else分支可能是block或if语句 */
        if (node->as.if_stmt.else_branch->type == AST_IF_STMT) {
            return xr_eval_if_stmt(X, node->as.if_stmt.else_branch, symbols, loop, ret);
        } else {
            return xr_eval_block_internal(X, node->as.if_stmt.else_branch, symbols, loop, ret);
        }
    }
    
    
        return xr_null();  /* 新API */

}

/*
** while 循环求值
*/
XrValue xr_eval_while_stmt(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    loop->loop_depth++;  /* 进入循环 */
    
    XrValue result = xr_null();  /* 新API - 默认初始化为null */

    
    while (1) {
        /* 求值条件 */
        XrValue condition = xr_eval_internal(X, node->as.while_stmt.condition, symbols, loop, ret);
        if (!xr_is_truthy(condition)) {
            break;  /* 条件为假，退出循环 */
        }
        
        /* 执行循环体 */
        result = xr_eval_block_internal(X, node->as.while_stmt.body, symbols, loop, ret);
        
        /* 检查 return */
        if (ret->has_returned) {
            break;
        }
        
        /* 检查 break/continue */
        if (loop->state == LOOP_BREAK) {
            loop->state = LOOP_NONE;  /* 清除状态 */
            break;
        } else if (loop->state == LOOP_CONTINUE) {
            loop->state = LOOP_NONE;  /* 清除状态 */
            continue;  /* 继续下次迭代 */
        }
    }
    
    loop->loop_depth--;  /* 退出循环 */
    return result;
}

/*
** for 循环求值
*/
XrValue xr_eval_for_stmt(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    /* 创建新作用域（for循环变量） */
    xsymboltable_begin_scope(symbols);
    
    /* 执行初始化 */
    if (node->as.for_stmt.initializer != NULL) {
        xr_eval_internal(X, node->as.for_stmt.initializer, symbols, loop, ret);
    }
    
    loop->loop_depth++;  /* 进入循环 */
    
    XrValue result = xr_null();  /* 新API - 默认初始化为null */

    
    while (1) {
        /* 检查条件 */
        if (node->as.for_stmt.condition != NULL) {
            XrValue condition = xr_eval_internal(X, node->as.for_stmt.condition, symbols, loop, ret);
            if (!xr_is_truthy(condition)) {
                break;  /* 条件为假，退出循环 */
            }
        }
        
        /* 执行循环体 */
        result = xr_eval_block_internal(X, node->as.for_stmt.body, symbols, loop, ret);
        
        /* 检查 return */
        if (ret->has_returned) {
            break;
        }
        
        /* 检查 break/continue */
        if (loop->state == LOOP_BREAK) {
            loop->state = LOOP_NONE;
            break;
        } else if (loop->state == LOOP_CONTINUE) {
            loop->state = LOOP_NONE;
            /* continue时要执行increment */
        }
        
        /* 执行更新 */
        if (node->as.for_stmt.increment != NULL) {
            xr_eval_internal(X, node->as.for_stmt.increment, symbols, loop, ret);
        }
    }
    
    loop->loop_depth--;  /* 退出循环 */
    
    /* 退出作用域 */
    xsymboltable_end_scope(symbols);
    
    return result;
}

/*
** break 语句求值
*/
XrValue xr_eval_break_stmt(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop) {
    (void)symbols;  /* 未使用 */
    
    /* 检查是否在循环中 */
    if (loop->loop_depth == 0) {
        xr_runtime_error(X, node->line, "break 只能在循环内使用");
    } else {
        loop->state = LOOP_BREAK;
    }
    
    
        return xr_null();  /* 新API */

}

/*
** continue 语句求值
*/
XrValue xr_eval_continue_stmt(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop) {
    (void)symbols;  /* 未使用 */
    
    /* 检查是否在循环中 */
    if (loop->loop_depth == 0) {
        xr_runtime_error(X, node->line, "continue 只能在循环内使用");
    } else {
        loop->state = LOOP_CONTINUE;
    }
    
    
        return xr_null();  /* 新API */

}

/* ========== 函数相关求值 ========== */

/*
** 求值函数声明
** 创建函数对象并注册到符号表
*/
XrValue xr_eval_function_decl(XrayState *X, AstNode *node, XSymbolTable *symbols) {
    FunctionDeclNode *func_node = &node->as.function_decl;
    
    /* 创建函数对象 */
    /* 注意：暂时传递NULL作为param_types和return_type，第6阶段后半段实现类型解析后再更新 */
    XrFunction *func = xr_function_new(func_node->name,
                                       func_node->parameters,
                                       NULL,  /* param_types - TODO: 实现类型解析后填充 */
                                       func_node->param_count,
                                       NULL,  /* return_type - TODO: 实现类型解析后填充 */
                                       func_node->body);
    
    if (func == NULL) {
        xr_runtime_error(X, node->line, "创建函数对象失败");
        
            return xr_null();  /* 新API */

    }
    
    /* ===== 第八阶段新增：捕获当前作用域（闭包支持） ===== */
    /* 保存当前作用域的引用，函数调用时会使用 */
    func->closure_scope = symbols->current;
    
    /* 创建函数值 */
    XrValue func_value = xr_null();  /* 初始化 */
    func_value.type = XR_TFUNCTION;
    func_value.type_info = NULL;  /* TODO: 函数类型信息 */
    func_value.as.obj = func;
    
    /* 将函数注册到符号表（作为变量） */
    if (!xsymboltable_define(symbols, func_node->name, func_value, false)) {
        xr_runtime_error(X, node->line, "函数名 '%s' 已被定义", func_node->name);
        xr_function_free(func);
        
            return xr_null();  /* 新API */

    }
    
    /* 返回 null（函数声明不返回值） */
    
        return xr_null();  /* 新API */

}

/*
** 求值函数表达式（箭头函数/匿名函数）
** 创建匿名函数对象并返回
*/
XrValue xr_eval_function_expr(XrayState *X, AstNode *node, XSymbolTable *symbols) {
    FunctionDeclNode *func_node = &node->as.function_expr;
    
    /* 创建匿名函数对象 */
    XrFunction *func = xr_function_new(NULL,  /* 匿名函数无名称 */
                                       func_node->parameters,
                                       NULL,  /* param_types */
                                       func_node->param_count,
                                       NULL,  /* return_type */
                                       func_node->body);
    
    if (func == NULL) {
        xr_runtime_error(X, node->line, "创建函数对象失败");
        return xr_null();
    }
    
    /* 捕获当前作用域（闭包支持） */
    func->closure_scope = symbols->current;
    
    /* 创建并返回函数值 */
    XrValue func_value = xr_null();
    func_value.type = XR_TFUNCTION;
    func_value.type_info = NULL;
    func_value.as.obj = func;
    
    return func_value;
}

/*
** 求值函数调用
** 执行函数并返回结果
*/
XrValue xr_eval_call_expr(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    CallExprNode *call_node = &node->as.call_expr;
    
    /* ===== 第九阶段新增：数组方法调用 ===== */
    /* 检查是否为方法调用（callee 是 AST_MEMBER_ACCESS） */
    if (call_node->callee->type == AST_MEMBER_ACCESS) {
        return xr_eval_array_method_call(X, node, symbols, loop, ret);
    }
    
    /* 求值被调用者（获取函数对象） */
    XrValue callee = xr_eval_internal(X, call_node->callee, symbols, loop, ret);
    
    /* 检查是否是函数 */
    if (!xr_isfunction(callee)) {
        xr_runtime_error(X, node->line, "只能调用函数");
        
            return xr_null();  /* 新API */

    }
    
    XrFunction *func = xr_tofunction(callee);
    
    /* 检查参数数量 */
    if (call_node->arg_count != func->param_count) {
        xr_runtime_error(X, node->line, 
            "函数 '%s' 期望 %d 个参数，但传入了 %d 个",
            func->name, func->param_count, call_node->arg_count);
        
            return xr_null();  /* 新API */

    }
    
    /* 求值所有参数 */
    XrValue *args = NULL;
    if (call_node->arg_count > 0) {
        args = (XrValue *)malloc(sizeof(XrValue) * call_node->arg_count);
        for (int i = 0; i < call_node->arg_count; i++) {
            args[i] = xr_eval_internal(X, call_node->arguments[i], symbols, loop, ret);
        }
    }
    
    /* ===== 第八阶段新增：支持闭包 ===== */
    /* 创建新的符号表用于函数执行，基于函数定义时的作用域（闭包） */
    XSymbolTable *func_symbols = xsymboltable_new();
    
    /* 如果函数有闭包作用域，设置为外层作用域 */
    if (func->closure_scope != NULL) {
        /* 设置闭包作用域为外层（重要：不由func_symbols拥有） */
        func_symbols->current->enclosing = func->closure_scope;
    }
    
    /* 绑定参数到函数作用域 */
    for (int i = 0; i < func->param_count; i++) {
        xsymboltable_define(func_symbols, func->parameters[i], args[i], false);
    }
    
    /* 创建新的返回控制 */
    ReturnControl local_ret = {0, {XR_TNULL}};
    local_ret.return_value = xr_null();  /* 新API */
    
    /* 执行函数体 */
    xr_eval_internal(X, func->body, func_symbols, loop, &local_ret);
    
    /* 获取返回值 */
    XrValue result = local_ret.has_returned ? local_ret.return_value : (XrValue){XR_TNULL};
    if (!local_ret.has_returned) {

    }
    
    /* ===== 关键修复：只释放当前作用域，不释放enclosing ===== */
    /* 保存enclosing引用 */
    XScope *enclosing_backup = func_symbols->current->enclosing;
    func_symbols->current->enclosing = NULL;  /* 断开链接 */
    
    /* 现在可以安全地释放func_symbols（只会释放自己创建的作用域） */
    xsymboltable_free(func_symbols);
    
    /* enclosing_backup仍然有效（由外部符号表管理） */
    
    /* 清理参数数组 */
    if (args != NULL) {
        free(args);
    }
    
    return result;
}

/*
** 求值 return 语句
** 设置返回控制状态
*/
XrValue xr_eval_return_stmt(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    ReturnStmtNode *return_node = &node->as.return_stmt;
    
    /* 求值返回值表达式 */
    if (return_node->value != NULL) {
        ret->return_value = xr_eval_internal(X, return_node->value, symbols, loop, ret);
    } else {
        ret->return_value = xr_null();  /* 新API */
    }
    
    /* 设置已返回标志 */
    ret->has_returned = 1;
    
    return ret->return_value;
}

/* ========== 数组求值函数（第九阶段新增）========== */

/*
** 求值数组字面量
** [1, 2, 3]
*/
XrValue xr_eval_array_literal(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    ArrayLiteralNode *arr_lit = &node->as.array_literal;
    
    /* 创建数组对象 */
    XrArray *arr = xr_array_new();
    
    /* 求值每个元素并添加到数组 */
    for (int i = 0; i < arr_lit->count; i++) {
        XrValue element = xr_eval_internal(X, arr_lit->elements[i], symbols, loop, ret);
        xr_array_push(arr, element);
    }
    
    /* 包装成 XrValue */
    return xr_value_from_array(arr);
}

/*
** 求值索引访问
** arr[0]
*/
XrValue xr_eval_index_get(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    IndexGetNode *get = &node->as.index_get;
    
    /* 求值数组表达式 */
    XrValue arr_val = xr_eval_internal(X, get->array, symbols, loop, ret);
    if (!xr_value_is_array(arr_val)) {
        xr_runtime_error(X, node->line, "无法对非数组值进行索引访问");
        return xr_null();
    }
    
    /* 求值索引表达式 */
    XrValue idx_val = xr_eval_internal(X, get->index, symbols, loop, ret);
    if (!xr_isint(idx_val)) {
        xr_runtime_error(X, node->line, "数组索引必须是整数");
        return xr_null();
    }
    
    /* 获取数组元素 */
    XrArray *arr = xr_value_to_array(arr_val);
    int index = xr_toint(idx_val);
    
    /* 边界检查（xr_array_get 内部会检查） */
    return xr_array_get(arr, index);
}

/*
** 求值索引赋值
** arr[0] = 10
*/
XrValue xr_eval_index_set(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    IndexSetNode *set = &node->as.index_set;
    
    /* 求值数组表达式 */
    XrValue arr_val = xr_eval_internal(X, set->array, symbols, loop, ret);
    if (!xr_value_is_array(arr_val)) {
        xr_runtime_error(X, node->line, "无法对非数组值进行索引赋值");
        return xr_null();
    }
    
    /* 求值索引表达式 */
    XrValue idx_val = xr_eval_internal(X, set->index, symbols, loop, ret);
    if (!xr_isint(idx_val)) {
        xr_runtime_error(X, node->line, "数组索引必须是整数");
        return xr_null();
    }
    
    /* 求值赋值表达式 */
    XrValue value = xr_eval_internal(X, set->value, symbols, loop, ret);
    
    /* 设置数组元素 */
    XrArray *arr = xr_value_to_array(arr_val);
    int index = xr_toint(idx_val);
    
    /* 边界检查 */
    if (index < 0 || index >= (int)arr->count) {
        xr_runtime_error(X, node->line, "数组索引越界: %d (数组长度: %zu)", index, arr->count);
        return xr_null();
    }
    
    xr_array_set(arr, index, value);
    
    /* 返回赋值结果 */
    return value;
}

/*
** 求值成员访问
** arr.length, arr.push (这个阶段只支持length属性)
*/
XrValue xr_eval_member_access(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    MemberAccessNode *member = &node->as.member_access;
    
    /* 求值对象表达式 */
    XrValue obj_val = xr_eval_internal(X, member->object, symbols, loop, ret);
    
    /* 数组对象 */
    if (xr_value_is_array(obj_val)) {
        XrArray *arr = xr_value_to_array(obj_val);
        
        /* length 属性 */
        if (strcmp(member->name, "length") == 0) {
            return xr_int(arr->count);
        }
        
        /* 其他方法暂不支持（方法调用在 AST_CALL_EXPR 中处理） */
        xr_runtime_error(X, node->line, "未知的数组属性: %s", member->name);
        return xr_null();
    }
    
    xr_runtime_error(X, node->line, "对象不支持成员访问");
    return xr_null();
}

/*
** 求值数组方法调用（第九阶段 Day 6 新增）
** arr.push(1), arr.pop(), arr.indexOf(10) 等
*/
XrValue xr_eval_array_method_call(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    CallExprNode *call_node = &node->as.call_expr;
    MemberAccessNode *member = &call_node->callee->as.member_access;
    
    /* 求值数组对象 */
    XrValue obj_val = xr_eval_internal(X, member->object, symbols, loop, ret);
    if (!xr_value_is_array(obj_val)) {
        xr_runtime_error(X, node->line, "只能在数组上调用数组方法");
        return xr_null();
    }
    
    XrArray *arr = xr_value_to_array(obj_val);
    const char *method = member->name;
    
    /* ===== push(element) ===== */
    if (strcmp(method, "push") == 0) {
        if (call_node->arg_count != 1) {
            xr_runtime_error(X, node->line, "push 方法需要 1 个参数");
            return xr_null();
        }
        XrValue arg = xr_eval_internal(X, call_node->arguments[0], symbols, loop, ret);
        xr_array_push(arr, arg);
        return xr_int(arr->count);  // 返回新长度
    }
    
    /* ===== pop() ===== */
    else if (strcmp(method, "pop") == 0) {
        if (call_node->arg_count != 0) {
            xr_runtime_error(X, node->line, "pop 方法不需要参数");
            return xr_null();
        }
        return xr_array_pop(arr);
    }
    
    /* ===== unshift(element) ===== */
    else if (strcmp(method, "unshift") == 0) {
        if (call_node->arg_count != 1) {
            xr_runtime_error(X, node->line, "unshift 方法需要 1 个参数");
            return xr_null();
        }
        XrValue arg = xr_eval_internal(X, call_node->arguments[0], symbols, loop, ret);
        xr_array_unshift(arr, arg);
        return xr_int(arr->count);  // 返回新长度
    }
    
    /* ===== shift() ===== */
    else if (strcmp(method, "shift") == 0) {
        if (call_node->arg_count != 0) {
            xr_runtime_error(X, node->line, "shift 方法不需要参数");
            return xr_null();
        }
        return xr_array_shift(arr);
    }
    
    /* ===== indexOf(element) ===== */
    else if (strcmp(method, "indexOf") == 0) {
        if (call_node->arg_count != 1) {
            xr_runtime_error(X, node->line, "indexOf 方法需要 1 个参数");
            return xr_null();
        }
        XrValue arg = xr_eval_internal(X, call_node->arguments[0], symbols, loop, ret);
        int index = xr_array_index_of(arr, arg);
        return xr_int(index);
    }
    
    /* ===== contains(element) ===== */
    else if (strcmp(method, "contains") == 0) {
        if (call_node->arg_count != 1) {
            xr_runtime_error(X, node->line, "contains 方法需要 1 个参数");
            return xr_null();
        }
        XrValue arg = xr_eval_internal(X, call_node->arguments[0], symbols, loop, ret);
        bool contains = xr_array_contains(arr, arg);
        return xr_bool(contains);
    }
    
    /* ===== forEach(callback) ===== */
    else if (strcmp(method, "forEach") == 0) {
        if (call_node->arg_count != 1) {
            xr_runtime_error(X, node->line, "forEach 方法需要 1 个参数");
            return xr_null();
        }
        XrValue callback_val = xr_eval_internal(X, call_node->arguments[0], symbols, loop, ret);
        if (!xr_isfunction(callback_val)) {
            xr_runtime_error(X, node->line, "forEach 需要一个函数作为参数");
            return xr_null();
        }
        XrFunction *callback = xr_tofunction(callback_val);
        xr_array_foreach(arr, callback, symbols);
        return xr_null();  // forEach 无返回值
    }
    
    /* ===== map(callback) ===== */
    else if (strcmp(method, "map") == 0) {
        if (call_node->arg_count != 1) {
            xr_runtime_error(X, node->line, "map 方法需要 1 个参数");
            return xr_null();
        }
        XrValue callback_val = xr_eval_internal(X, call_node->arguments[0], symbols, loop, ret);
        if (!xr_isfunction(callback_val)) {
            xr_runtime_error(X, node->line, "map 需要一个函数作为参数");
            return xr_null();
        }
        XrFunction *callback = xr_tofunction(callback_val);
        XrArray *result = xr_array_map(arr, callback, symbols);
        return xr_value_from_array(result);
    }
    
    /* ===== filter(callback) ===== */
    else if (strcmp(method, "filter") == 0) {
        if (call_node->arg_count != 1) {
            xr_runtime_error(X, node->line, "filter 方法需要 1 个参数");
            return xr_null();
        }
        XrValue callback_val = xr_eval_internal(X, call_node->arguments[0], symbols, loop, ret);
        if (!xr_isfunction(callback_val)) {
            xr_runtime_error(X, node->line, "filter 需要一个函数作为参数");
            return xr_null();
        }
        XrFunction *callback = xr_tofunction(callback_val);
        XrArray *result = xr_array_filter(arr, callback, symbols);
        return xr_value_from_array(result);
    }
    
    /* ===== reduce(callback, initial) ===== */
    else if (strcmp(method, "reduce") == 0) {
        if (call_node->arg_count != 2) {
            xr_runtime_error(X, node->line, "reduce 方法需要 2 个参数");
            return xr_null();
        }
        XrValue callback_val = xr_eval_internal(X, call_node->arguments[0], symbols, loop, ret);
        if (!xr_isfunction(callback_val)) {
            xr_runtime_error(X, node->line, "reduce 需要一个函数作为第一个参数");
            return xr_null();
        }
        XrValue initial = xr_eval_internal(X, call_node->arguments[1], symbols, loop, ret);
        XrFunction *callback = xr_tofunction(callback_val);
        return xr_array_reduce(arr, callback, initial, symbols);
    }
    
    /* 未知方法 */
    else {
        xr_runtime_error(X, node->line, "未知的数组方法: %s", method);
        return xr_null();
    }
}

/*
** 辅助函数：调用函数（用于高阶方法）
** 简化版本，用于数组方法的回调
*/
XrValue xr_eval_call_function(XrFunction *func, XrValue *args, int arg_count, XSymbolTable *parent_symbols) {
    /* 创建新的符号表用于函数执行 */
    XSymbolTable *func_symbols = xsymboltable_new();
    
    /* 设置闭包作用域 */
    if (func->closure_scope != NULL) {
        func_symbols->current->enclosing = func->closure_scope;
    }
    
    /* 绑定参数（只绑定实际传入的参数数量） */
    int params_to_bind = (arg_count < func->param_count) ? arg_count : func->param_count;
    for (int i = 0; i < params_to_bind; i++) {
        xsymboltable_define(func_symbols, func->parameters[i], args[i], false);
    }
    
    /* 创建返回控制 */
    ReturnControl ret_ctrl = {0, {XR_TNULL}};
    ret_ctrl.return_value = xr_null();
    
    LoopControl loop_ctrl = {LOOP_NONE, 0};
    
    /* 执行函数体 */
    XrValue result = xr_null();
    if (func->body != NULL) {
        result = xr_eval_internal(NULL, func->body, func_symbols, &loop_ctrl, &ret_ctrl);
        
        /* 如果有返回值，使用返回值 */
        if (ret_ctrl.has_returned) {
            result = ret_ctrl.return_value;
        }
    }
    
    /* 断开闭包链接（避免释放共享的作用域） */
    if (func->closure_scope != NULL) {
        func_symbols->current->enclosing = NULL;
    }
    
    /* 清理符号表 */
    xsymboltable_free(func_symbols);
    
    return result;
}

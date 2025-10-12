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
        XrValue error_value;
        xr_setnull(&error_value);
        return error_value;
    }
    
    /* 初始化循环控制和返回控制 */
    LoopControl loop = {LOOP_NONE, 0};
    ReturnControl ret = {0, {XR_TNULL}};
    xr_setnull(&ret.return_value);
    
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
        XrValue error_value;
        xr_setnull(&error_value);
        return error_value;
    }
    
    /* 初始化循环控制和返回控制 */
    LoopControl loop = {LOOP_NONE, 0};
    ReturnControl ret = {0, {XR_TNULL}};
    xr_setnull(&ret.return_value);
    
    return xr_eval_internal(X, node, symbols, &loop, &ret);
}

/*
** 内部求值函数 - AST 访问者模式的核心
** 根据节点类型分发到相应的处理函数
*/
static XrValue xr_eval_internal(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    if (node == NULL) {
        XrValue null_value;
        xr_setnull(&null_value);
        return null_value;
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
        
        case AST_CALL_EXPR:
            return xr_eval_call_expr(X, node, symbols, loop, ret);
        
        case AST_RETURN_STMT:
            return xr_eval_return_stmt(X, node, symbols, loop, ret);
        
        case AST_PROGRAM:
            return xr_eval_program(X, node, symbols, loop, ret);
        
        default:
            xr_runtime_error(X, node->line, "未知的 AST 节点类型: %d", node->type);
            XrValue error_value;
            xr_setnull(&error_value);
            return error_value;
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
            XrValue error_value;
            xr_setnull(&error_value);
            return error_value;
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
            XrValue error_value;
            xr_setnull(&error_value);
            return error_value;
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
    XrValue null_value;
    xr_setnull(&null_value);
    return null_value;
}

/*
** 程序求值（执行多个语句）
*/
static XrValue xr_eval_program(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    XrValue last_value;
    xr_setnull(&last_value);
    
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
    XrValue result;
    
    /* 字符串拼接 */
    if (xr_isstring(&left) && xr_isstring(&right)) {
        /* 简单实现：字符串拼接 */
        const char *left_str = (const char *)xr_toobj(&left);
        const char *right_str = (const char *)xr_toobj(&right);
        
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
        if (xr_isint(&left) && xr_isint(&right)) {
            xr_setint(&result, (xr_Integer)(left_num + right_num));
        } else {
            xr_setfloat(&result, left_num + right_num);
        }
        return result;
    }
    
    xr_runtime_error(X, 0, "加法运算的操作数必须都是数字或都是字符串");
    xr_setnull(&result);
    return result;
}

/*
** 减法运算
*/
XrValue xr_eval_subtract(XrayState *X, XrValue left, XrValue right) {
    XrValue result;
    
    if (!xr_is_number(left) || !xr_is_number(right)) {
        xr_runtime_error(X, 0, "减法运算的操作数必须是数字");
        xr_setnull(&result);
        return result;
    }
    
    xr_Number left_num = xr_to_number(X, left);
    xr_Number right_num = xr_to_number(X, right);
    
    if (xr_isint(&left) && xr_isint(&right)) {
        xr_setint(&result, (xr_Integer)(left_num - right_num));
    } else {
        xr_setfloat(&result, left_num - right_num);
    }
    
    return result;
}

/*
** 乘法运算
*/
XrValue xr_eval_multiply(XrayState *X, XrValue left, XrValue right) {
    XrValue result;
    
    if (!xr_is_number(left) || !xr_is_number(right)) {
        xr_runtime_error(X, 0, "乘法运算的操作数必须是数字");
        xr_setnull(&result);
        return result;
    }
    
    xr_Number left_num = xr_to_number(X, left);
    xr_Number right_num = xr_to_number(X, right);
    
    if (xr_isint(&left) && xr_isint(&right)) {
        xr_setint(&result, (xr_Integer)(left_num * right_num));
    } else {
        xr_setfloat(&result, left_num * right_num);
    }
    
    return result;
}

/*
** 除法运算
*/
XrValue xr_eval_divide(XrayState *X, XrValue left, XrValue right) {
    XrValue result;
    
    if (!xr_is_number(left) || !xr_is_number(right)) {
        xr_runtime_error(X, 0, "除法运算的操作数必须是数字");
        xr_setnull(&result);
        return result;
    }
    
    xr_Number left_num = xr_to_number(X, left);
    xr_Number right_num = xr_to_number(X, right);
    
    if (right_num == 0.0) {
        xr_runtime_error(X, 0, "除零错误");
        xr_setnull(&result);
        return result;
    }
    
    /* 除法结果总是浮点数 */
    xr_setfloat(&result, left_num / right_num);
    return result;
}

/*
** 取模运算
*/
XrValue xr_eval_modulo(XrayState *X, XrValue left, XrValue right) {
    XrValue result;
    
    if (!xr_is_number(left) || !xr_is_number(right)) {
        xr_runtime_error(X, 0, "取模运算的操作数必须是数字");
        xr_setnull(&result);
        return result;
    }
    
    xr_Number left_num = xr_to_number(X, left);
    xr_Number right_num = xr_to_number(X, right);
    
    if (right_num == 0.0) {
        xr_runtime_error(X, 0, "取模运算的除数不能为零");
        xr_setnull(&result);
        return result;
    }
    
    if (xr_isint(&left) && xr_isint(&right)) {
        xr_setint(&result, xr_toint(&left) % xr_toint(&right));
    } else {
        xr_setfloat(&result, fmod(left_num, right_num));
    }
    
    return result;
}

/* ========== 比较运算实现 ========== */

/*
** 相等比较
*/
XrValue xr_eval_equal(XrayState *X, XrValue left, XrValue right) {
    XrValue result;
    xr_setbool(&result, xr_values_equal(left, right));
    return result;
}

/*
** 不等比较
*/
XrValue xr_eval_not_equal(XrayState *X, XrValue left, XrValue right) {
    XrValue result;
    xr_setbool(&result, !xr_values_equal(left, right));
    return result;
}

/*
** 小于比较
*/
XrValue xr_eval_less(XrayState *X, XrValue left, XrValue right) {
    XrValue result;
    
    if (!xr_is_number(left) || !xr_is_number(right)) {
        xr_runtime_error(X, 0, "比较运算的操作数必须是数字");
        xr_setnull(&result);
        return result;
    }
    
    xr_Number left_num = xr_to_number(X, left);
    xr_Number right_num = xr_to_number(X, right);
    
    xr_setbool(&result, left_num < right_num);
    return result;
}

/*
** 小于等于比较
*/
XrValue xr_eval_less_equal(XrayState *X, XrValue left, XrValue right) {
    XrValue result;
    
    if (!xr_is_number(left) || !xr_is_number(right)) {
        xr_runtime_error(X, 0, "比较运算的操作数必须是数字");
        xr_setnull(&result);
        return result;
    }
    
    xr_Number left_num = xr_to_number(X, left);
    xr_Number right_num = xr_to_number(X, right);
    
    xr_setbool(&result, left_num <= right_num);
    return result;
}

/*
** 大于比较
*/
XrValue xr_eval_greater(XrayState *X, XrValue left, XrValue right) {
    XrValue result;
    
    if (!xr_is_number(left) || !xr_is_number(right)) {
        xr_runtime_error(X, 0, "比较运算的操作数必须是数字");
        xr_setnull(&result);
        return result;
    }
    
    xr_Number left_num = xr_to_number(X, left);
    xr_Number right_num = xr_to_number(X, right);
    
    xr_setbool(&result, left_num > right_num);
    return result;
}

/*
** 大于等于比较
*/
XrValue xr_eval_greater_equal(XrayState *X, XrValue left, XrValue right) {
    XrValue result;
    
    if (!xr_is_number(left) || !xr_is_number(right)) {
        xr_runtime_error(X, 0, "比较运算的操作数必须是数字");
        xr_setnull(&result);
        return result;
    }
    
    xr_Number left_num = xr_to_number(X, left);
    xr_Number right_num = xr_to_number(X, right);
    
    xr_setbool(&result, left_num >= right_num);
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
    XrValue result;
    xr_setbool(&result, !xr_is_truthy(operand));
    return result;
}

/*
** 取负运算
*/
XrValue xr_eval_negate(XrayState *X, XrValue operand) {
    XrValue result;
    
    if (!xr_is_number(operand)) {
        xr_runtime_error(X, 0, "取负运算的操作数必须是数字");
        xr_setnull(&result);
        return result;
    }
    
    if (xr_isint(&operand)) {
        xr_setint(&result, -xr_toint(&operand));
    } else {
        xr_setfloat(&result, -xr_tofloat(&operand));
    }
    
    return result;
}

/* ========== 辅助函数实现 ========== */

/*
** 检查值是否为数字
*/
int xr_is_number(XrValue value) {
    return xr_isint(&value) || xr_isfloat(&value);
}

/*
** 检查值的真假性
** 规则：null 和 false 为假，其他都为真
*/
int xr_is_truthy(XrValue value) {
    if (xr_isnull(&value)) return 0;
    if (xr_isbool(&value)) return xr_tobool(&value);
    return 1;  /* 其他值都为真 */
}

/*
** 将值转换为数字
*/
xr_Number xr_to_number(XrayState *X, XrValue value) {
    if (xr_isint(&value)) {
        return (xr_Number)xr_toint(&value);
    } else if (xr_isfloat(&value)) {
        return xr_tofloat(&value);
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
        if ((xr_isint(&a) && xr_isfloat(&b)) || (xr_isfloat(&a) && xr_isint(&b))) {
            xr_Number a_num = xr_isint(&a) ? (xr_Number)xr_toint(&a) : xr_tofloat(&a);
            xr_Number b_num = xr_isint(&b) ? (xr_Number)xr_toint(&b) : xr_tofloat(&b);
            return a_num == b_num;
        }
        return 0;
    }
    
    /* 根据类型比较 */
    switch (a.type) {
        case XR_TNULL:   return 1;  /* null == null */
        case XR_TBOOL:   return xr_tobool(&a) == xr_tobool(&b);
        case XR_TINT:    return xr_toint(&a) == xr_toint(&b);
        case XR_TFLOAT:  return xr_tofloat(&a) == xr_tofloat(&b);
        case XR_TSTRING:
            return strcmp((const char *)xr_toobj(&a), (const char *)xr_toobj(&b)) == 0;
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
            return xr_tobool(&value) ? "true" : "false";
        case XR_TINT:
            snprintf(buffer, sizeof(buffer), "%lld", (long long)xr_toint(&value));
            return buffer;
        case XR_TFLOAT:
            snprintf(buffer, sizeof(buffer), "%g", xr_tofloat(&value));
            return buffer;
        case XR_TSTRING:
            return (const char *)xr_toobj(&value);
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
        xr_setnull(&init_value);
    }
    
    /* 在当前作用域定义变量 */
    if (!xsymboltable_define(symbols, name, init_value, is_const)) {
        xr_runtime_error(X, node->line, "变量 '%s' 已定义", name);
    }
    
    /* 返回 null */
    XrValue null_value;
    xr_setnull(&null_value);
    return null_value;
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
        xr_setnull(&value);
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
        xr_setnull(&value);
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
    
    XrValue last_value;
    xr_setnull(&last_value);
    
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
    xr_setnull(&ret.return_value);
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
    
    XrValue null_value;
    xr_setnull(&null_value);
    return null_value;
}

/*
** while 循环求值
*/
XrValue xr_eval_while_stmt(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    loop->loop_depth++;  /* 进入循环 */
    
    XrValue result;
    xr_setnull(&result);
    
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
    
    XrValue result;
    xr_setnull(&result);
    
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
    
    XrValue null_value;
    xr_setnull(&null_value);
    return null_value;
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
    
    XrValue null_value;
    xr_setnull(&null_value);
    return null_value;
}

/* ========== 函数相关求值 ========== */

/*
** 求值函数声明
** 创建函数对象并注册到符号表
*/
XrValue xr_eval_function_decl(XrayState *X, AstNode *node, XSymbolTable *symbols) {
    FunctionDeclNode *func_node = &node->as.function_decl;
    
    /* 创建函数对象 */
    XrFunction *func = xr_function_new(func_node->name, 
                                       func_node->parameters,
                                       func_node->param_count,
                                       func_node->body);
    
    if (func == NULL) {
        xr_runtime_error(X, node->line, "创建函数对象失败");
        XrValue error_value;
        xr_setnull(&error_value);
        return error_value;
    }
    
    /* 创建函数值 */
    XrValue func_value;
    xr_setfunction(&func_value, func);
    
    /* 将函数注册到符号表（作为变量） */
    if (!xsymboltable_define(symbols, func_node->name, func_value, false)) {
        xr_runtime_error(X, node->line, "函数名 '%s' 已被定义", func_node->name);
        xr_function_free(func);
        XrValue error_value;
        xr_setnull(&error_value);
        return error_value;
    }
    
    /* 返回 null（函数声明不返回值） */
    XrValue null_value;
    xr_setnull(&null_value);
    return null_value;
}

/*
** 求值函数调用
** 执行函数并返回结果
*/
XrValue xr_eval_call_expr(XrayState *X, AstNode *node, XSymbolTable *symbols, LoopControl *loop, ReturnControl *ret) {
    CallExprNode *call_node = &node->as.call_expr;
    
    /* 求值被调用者（获取函数对象） */
    XrValue callee = xr_eval_internal(X, call_node->callee, symbols, loop, ret);
    
    /* 检查是否是函数 */
    if (!xr_isfunction(&callee)) {
        xr_runtime_error(X, node->line, "只能调用函数");
        XrValue error_value;
        xr_setnull(&error_value);
        return error_value;
    }
    
    XrFunction *func = xr_tofunction(&callee);
    
    /* 检查参数数量 */
    if (call_node->arg_count != func->param_count) {
        xr_runtime_error(X, node->line, 
            "函数 '%s' 期望 %d 个参数，但传入了 %d 个",
            func->name, func->param_count, call_node->arg_count);
        XrValue error_value;
        xr_setnull(&error_value);
        return error_value;
    }
    
    /* 求值所有参数 */
    XrValue *args = NULL;
    if (call_node->arg_count > 0) {
        args = (XrValue *)malloc(sizeof(XrValue) * call_node->arg_count);
        for (int i = 0; i < call_node->arg_count; i++) {
            args[i] = xr_eval_internal(X, call_node->arguments[i], symbols, loop, ret);
        }
    }
    
    /* 在当前符号表中创建新作用域（这样可以访问外层函数） */
    xsymboltable_begin_scope(symbols);
    
    /* 绑定参数到当前作用域 */
    for (int i = 0; i < func->param_count; i++) {
        xsymboltable_define(symbols, func->parameters[i], args[i], false);
    }
    
    /* 创建新的返回控制 */
    ReturnControl local_ret = {0, {XR_TNULL}};
    xr_setnull(&local_ret.return_value);
    
    /* 执行函数体 */
    xr_eval_internal(X, func->body, symbols, loop, &local_ret);
    
    /* 获取返回值 */
    XrValue result = local_ret.has_returned ? local_ret.return_value : (XrValue){XR_TNULL};
    if (!local_ret.has_returned) {
        xr_setnull(&result);
    }
    
    /* 退出作用域 */
    xsymboltable_end_scope(symbols);
    
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
        xr_setnull(&ret->return_value);
    }
    
    /* 设置已返回标志 */
    ret->has_returned = 1;
    
    return ret->return_value;
}

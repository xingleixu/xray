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

/* ========== 主要求值函数 ========== */

/*
** 主求值函数 - AST 访问者模式的核心
** 根据节点类型分发到相应的处理函数
*/
XrValue xr_eval(XrayState *X, AstNode *node) {
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
            return xr_eval_binary(X, node);
        
        /* 一元运算节点 */
        case AST_UNARY_NEG:
        case AST_UNARY_NOT:
            return xr_eval_unary(X, node);
        
        /* 分组节点 */
        case AST_GROUPING:
            return xr_eval_grouping(X, node);
        
        /* 语句节点 */
        case AST_EXPR_STMT:
            return xr_eval_expr_stmt(X, node);
        
        case AST_PRINT_STMT:
            return xr_eval_print_stmt(X, node);
        
        case AST_PROGRAM:
            return xr_eval_program(X, node);
        
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
XrValue xr_eval_literal(XrayState *X, AstNode *node) {
    return node->as.literal.value;
}

/*
** 二元运算求值
*/
XrValue xr_eval_binary(XrayState *X, AstNode *node) {
    /* 逻辑运算需要短路求值，特殊处理 */
    if (node->type == AST_BINARY_AND) {
        return xr_eval_logical_and(X, node->as.binary.left, node->as.binary.right);
    }
    if (node->type == AST_BINARY_OR) {
        return xr_eval_logical_or(X, node->as.binary.left, node->as.binary.right);
    }
    
    /* 其他运算：先求值两个操作数 */
    XrValue left = xr_eval(X, node->as.binary.left);
    XrValue right = xr_eval(X, node->as.binary.right);
    
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
XrValue xr_eval_unary(XrayState *X, AstNode *node) {
    XrValue operand = xr_eval(X, node->as.unary.operand);
    
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
XrValue xr_eval_grouping(XrayState *X, AstNode *node) {
    return xr_eval(X, node->as.grouping);
}

/*
** 表达式语句求值
*/
XrValue xr_eval_expr_stmt(XrayState *X, AstNode *node) {
    return xr_eval(X, node->as.expr_stmt);
}

/*
** print 语句求值
*/
XrValue xr_eval_print_stmt(XrayState *X, AstNode *node) {
    XrValue value = xr_eval(X, node->as.print_stmt.expr);
    
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
XrValue xr_eval_program(XrayState *X, AstNode *node) {
    XrValue last_value;
    xr_setnull(&last_value);
    
    /* 执行所有语句，返回最后一个语句的值 */
    for (int i = 0; i < node->as.program.count; i++) {
        last_value = xr_eval(X, node->as.program.statements[i]);
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
XrValue xr_eval_logical_and(XrayState *X, AstNode *left_node, AstNode *right_node) {
    XrValue left = xr_eval(X, left_node);
    
    /* 如果左边为假，直接返回左边的值（短路） */
    if (!xr_is_truthy(left)) {
        return left;
    }
    
    /* 否则返回右边的值 */
    return xr_eval(X, right_node);
}

/*
** 逻辑或（短路求值）
*/
XrValue xr_eval_logical_or(XrayState *X, AstNode *left_node, AstNode *right_node) {
    XrValue left = xr_eval(X, left_node);
    
    /* 如果左边为真，直接返回左边的值（短路） */
    if (xr_is_truthy(left)) {
        return left;
    }
    
    /* 否则返回右边的值 */
    return xr_eval(X, right_node);
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

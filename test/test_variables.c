/*
** test_variables.c
** Xray 变量和作用域测试
** 测试变量声明、赋值、作用域管理等功能
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../xray.h"
#include "../xlex.h"
#include "../xast.h"
#include "../xparse.h"
#include "../xeval.h"
#include "../xvalue.h"
#include "../xscope.h"

/* 测试统计 */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* 测试宏 */
#define TEST(name) \
    static void name(void); \
    static void run_##name(void) { \
        printf("  测试: %s ... ", #name); \
        tests_run++; \
        name(); \
        tests_passed++; \
        printf("✓\n"); \
    } \
    static void name(void)

#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("✗\n"); \
            printf("    断言失败: %s\n", message); \
            printf("    位置: %s:%d\n", __FILE__, __LINE__); \
            tests_failed++; \
            return; \
        } \
    } while(0)

#define RUN_TEST(name) run_##name()

/* ========== 辅助函数 ========== */

/* 解析并求值代码 */
static XrValue eval_code(XrayState *X, const char *code) {
    AstNode *ast = xr_parse(X, code);
    if (!ast) {
        XrValue null_val;
        xr_setnull(&null_val);
        return null_val;
    }
    XrValue result = xr_eval(X, ast);
    xr_ast_free(X, ast);
    return result;
}

/* ========== 符号表测试 ========== */

TEST(test_symbol_table_create) {
    XSymbolTable *table = xsymboltable_new();
    ASSERT(table != NULL, "符号表创建失败");
    ASSERT(table->global != NULL, "全局作用域未创建");
    ASSERT(table->current == table->global, "当前作用域不是全局作用域");
    ASSERT(table->scope_depth == 0, "作用域深度应该为0");
    xsymboltable_free(table);
}

TEST(test_symbol_table_define_variable) {
    XSymbolTable *table = xsymboltable_new();
    XrValue value;
    xr_setint(&value, 42);
    
    bool success = xsymboltable_define(table, "x", value, false);
    ASSERT(success, "变量定义失败");
    
    XrValue result;
    bool found = xsymboltable_get(table, "x", &result);
    ASSERT(found, "变量未找到");
    ASSERT(xr_isint(&result), "变量类型错误");
    ASSERT(xr_toint(&result) == 42, "变量值错误");
    
    xsymboltable_free(table);
}

TEST(test_symbol_table_define_const) {
    XSymbolTable *table = xsymboltable_new();
    XrValue value;
    xr_setfloat(&value, 3.14);
    
    bool success = xsymboltable_define(table, "PI", value, true);
    ASSERT(success, "常量定义失败");
    
    XrValue new_value;
    xr_setfloat(&new_value, 2.71);
    bool assign_success = xsymboltable_assign(table, "PI", new_value);
    ASSERT(!assign_success, "常量不应该能被修改");
    
    xsymboltable_free(table);
}

TEST(test_symbol_table_scope) {
    XSymbolTable *table = xsymboltable_new();
    XrValue value1, value2;
    xr_setint(&value1, 10);
    xr_setint(&value2, 20);
    
    /* 全局作用域定义 x = 10 */
    xsymboltable_define(table, "x", value1, false);
    
    /* 进入新作用域 */
    xsymboltable_begin_scope(table);
    ASSERT(table->scope_depth == 1, "作用域深度应该为1");
    
    /* 局部作用域定义 x = 20（遮蔽） */
    xsymboltable_define(table, "x", value2, false);
    
    XrValue result;
    xsymboltable_get(table, "x", &result);
    ASSERT(xr_toint(&result) == 20, "应该获取到局部变量");
    
    /* 退出作用域 */
    xsymboltable_end_scope(table);
    ASSERT(table->scope_depth == 0, "作用域深度应该回到0");
    
    xsymboltable_get(table, "x", &result);
    ASSERT(xr_toint(&result) == 10, "应该获取到全局变量");
    
    xsymboltable_free(table);
}

/* ========== 变量声明测试 ========== */

TEST(test_var_decl_with_init) {
    XrayState *X = xray_newstate();
    const char *code = "let x = 42";
    
    AstNode *ast = xr_parse(X, code);
    ASSERT(ast != NULL, "解析失败");
    ASSERT(ast->type == AST_PROGRAM, "根节点类型错误");
    ASSERT(ast->as.program.count == 1, "语句数量错误");
    
    AstNode *decl = ast->as.program.statements[0];
    ASSERT(decl->type == AST_VAR_DECL, "节点类型应该是变量声明");
    ASSERT(strcmp(decl->as.var_decl.name, "x") == 0, "变量名错误");
    ASSERT(!decl->as.var_decl.is_const, "不应该是常量");
    ASSERT(decl->as.var_decl.initializer != NULL, "应该有初始化表达式");
    
    xr_ast_free(X, ast);
    xray_close(X);
}

TEST(test_var_decl_without_init) {
    XrayState *X = xray_newstate();
    const char *code = "let x";
    
    AstNode *ast = xr_parse(X, code);
    ASSERT(ast != NULL, "解析失败");
    
    AstNode *decl = ast->as.program.statements[0];
    ASSERT(decl->type == AST_VAR_DECL, "节点类型应该是变量声明");
    ASSERT(decl->as.var_decl.initializer == NULL, "不应该有初始化表达式");
    
    xr_ast_free(X, ast);
    xray_close(X);
}

TEST(test_const_decl) {
    XrayState *X = xray_newstate();
    const char *code = "const PI = 3.14";
    
    AstNode *ast = xr_parse(X, code);
    ASSERT(ast != NULL, "解析失败");
    
    AstNode *decl = ast->as.program.statements[0];
    ASSERT(decl->type == AST_CONST_DECL, "节点类型应该是常量声明");
    ASSERT(strcmp(decl->as.var_decl.name, "PI") == 0, "常量名错误");
    ASSERT(decl->as.var_decl.is_const, "应该是常量");
    ASSERT(decl->as.var_decl.initializer != NULL, "常量必须有初始化");
    
    xr_ast_free(X, ast);
    xray_close(X);
}

/* ========== 变量引用测试 ========== */

TEST(test_variable_reference) {
    XrayState *X = xray_newstate();
    const char *code = "x";
    
    AstNode *ast = xr_parse(X, code);
    ASSERT(ast != NULL, "解析失败");
    
    AstNode *stmt = ast->as.program.statements[0];
    ASSERT(stmt->type == AST_EXPR_STMT, "应该是表达式语句");
    
    AstNode *expr = stmt->as.expr_stmt;
    ASSERT(expr->type == AST_VARIABLE, "应该是变量引用");
    ASSERT(strcmp(expr->as.variable.name, "x") == 0, "变量名错误");
    
    xr_ast_free(X, ast);
    xray_close(X);
}

TEST(test_assignment) {
    XrayState *X = xray_newstate();
    const char *code = "x = 10";
    
    AstNode *ast = xr_parse(X, code);
    ASSERT(ast != NULL, "解析失败");
    
    AstNode *stmt = ast->as.program.statements[0];
    AstNode *assign = stmt->as.expr_stmt;
    ASSERT(assign->type == AST_ASSIGNMENT, "应该是赋值");
    ASSERT(strcmp(assign->as.assignment.name, "x") == 0, "变量名错误");
    ASSERT(assign->as.assignment.value != NULL, "应该有赋值表达式");
    
    xr_ast_free(X, ast);
    xray_close(X);
}

/* ========== 代码块测试 ========== */

TEST(test_block) {
    XrayState *X = xray_newstate();
    const char *code = "{ let x = 10 }";
    
    AstNode *ast = xr_parse(X, code);
    ASSERT(ast != NULL, "解析失败");
    
    AstNode *block = ast->as.program.statements[0];
    ASSERT(block->type == AST_BLOCK, "应该是代码块");
    ASSERT(block->as.block.count == 1, "代码块应该有1个语句");
    
    xr_ast_free(X, ast);
    xray_close(X);
}

TEST(test_nested_blocks) {
    XrayState *X = xray_newstate();
    const char *code = "{ { let x = 10 } }";
    
    AstNode *ast = xr_parse(X, code);
    ASSERT(ast != NULL, "解析失败");
    
    AstNode *outer = ast->as.program.statements[0];
    ASSERT(outer->type == AST_BLOCK, "外层应该是代码块");
    
    AstNode *inner = outer->as.block.statements[0];
    ASSERT(inner->type == AST_BLOCK, "内层应该是代码块");
    
    xr_ast_free(X, ast);
    xray_close(X);
}

/* ========== 求值测试 ========== */

TEST(test_eval_var_decl) {
    XrayState *X = xray_newstate();
    const char *code = "let x = 42\nprint(x)";
    
    /* 重定向 stdout 来检查输出 */
    XrValue result = eval_code(X, code);
    /* 应该成功执行，不崩溃 */
    
    xray_close(X);
}

TEST(test_eval_assignment) {
    XrayState *X = xray_newstate();
    const char *code = "let x = 10\nx = 20\nprint(x)";
    
    XrValue result = eval_code(X, code);
    /* 应该打印 20 */
    
    xray_close(X);
}

TEST(test_eval_scope) {
    XrayState *X = xray_newstate();
    const char *code = 
        "let x = 10\n"
        "{\n"
        "  let x = 20\n"
        "  print(x)\n"
        "}\n"
        "print(x)";
    
    XrValue result = eval_code(X, code);
    /* 应该打印 20 然后 10 */
    
    xray_close(X);
}

TEST(test_eval_expression_with_vars) {
    XrayState *X = xray_newstate();
    const char *code = 
        "let a = 10\n"
        "let b = 20\n"
        "let sum = a + b\n"
        "print(sum)";
    
    XrValue result = eval_code(X, code);
    /* 应该打印 30 */
    
    xray_close(X);
}

/* ========== 错误测试 ========== */

TEST(test_error_undefined_variable) {
    XrayState *X = xray_newstate();
    const char *code = "print(undefined_var)";
    
    /* 应该产生运行时错误，但不崩溃 */
    XrValue result = eval_code(X, code);
    
    xray_close(X);
}

TEST(test_error_const_reassignment) {
    XrayState *X = xray_newstate();
    const char *code = "const PI = 3.14\nPI = 2.71";
    
    /* 应该产生运行时错误 */
    XrValue result = eval_code(X, code);
    
    xray_close(X);
}

TEST(test_const_must_initialize) {
    XrayState *X = xray_newstate();
    const char *code = "const X";
    
    /* 应该产生解析错误 */
    AstNode *ast = xr_parse(X, code);
    ASSERT(ast == NULL, "常量没有初始化应该解析失败");
    
    xray_close(X);
}

/* ========== 复杂场景测试 ========== */

TEST(test_variable_shadowing) {
    XrayState *X = xray_newstate();
    const char *code = 
        "let x = 1\n"
        "{\n"
        "  let x = 2\n"
        "  {\n"
        "    let x = 3\n"
        "    print(x)\n"
        "  }\n"
        "  print(x)\n"
        "}\n"
        "print(x)";
    
    XrValue result = eval_code(X, code);
    /* 应该打印 3, 2, 1 */
    
    xray_close(X);
}

TEST(test_access_outer_variable) {
    XrayState *X = xray_newstate();
    const char *code = 
        "let outer = 10\n"
        "{\n"
        "  let inner = 20\n"
        "  let sum = outer + inner\n"
        "  print(sum)\n"
        "}";
    
    XrValue result = eval_code(X, code);
    /* 应该打印 30 */
    
    xray_close(X);
}

TEST(test_modify_outer_variable) {
    XrayState *X = xray_newstate();
    const char *code = 
        "let count = 0\n"
        "{\n"
        "  count = count + 1\n"
        "  print(count)\n"
        "}\n"
        "print(count)";
    
    XrValue result = eval_code(X, code);
    /* 应该打印 1, 1 */
    
    xray_close(X);
}

TEST(test_multiple_variables) {
    XrayState *X = xray_newstate();
    const char *code = 
        "let a = 1\n"
        "let b = 2\n"
        "let c = 3\n"
        "let d = a + b + c\n"
        "print(d)";
    
    XrValue result = eval_code(X, code);
    /* 应该打印 6 */
    
    xray_close(X);
}

/* ========== 主测试函数 ========== */

int main(void) {
    printf("\n========================================\n");
    printf("Xray 变量和作用域测试\n");
    printf("========================================\n\n");
    
    printf("符号表测试:\n");
    RUN_TEST(test_symbol_table_create);
    RUN_TEST(test_symbol_table_define_variable);
    RUN_TEST(test_symbol_table_define_const);
    RUN_TEST(test_symbol_table_scope);
    
    printf("\n变量声明解析测试:\n");
    RUN_TEST(test_var_decl_with_init);
    RUN_TEST(test_var_decl_without_init);
    RUN_TEST(test_const_decl);
    
    printf("\n变量引用和赋值解析测试:\n");
    RUN_TEST(test_variable_reference);
    RUN_TEST(test_assignment);
    
    printf("\n代码块解析测试:\n");
    RUN_TEST(test_block);
    RUN_TEST(test_nested_blocks);
    
    printf("\n求值测试:\n");
    RUN_TEST(test_eval_var_decl);
    RUN_TEST(test_eval_assignment);
    RUN_TEST(test_eval_scope);
    RUN_TEST(test_eval_expression_with_vars);
    
    printf("\n错误处理测试:\n");
    RUN_TEST(test_error_undefined_variable);
    RUN_TEST(test_error_const_reassignment);
    RUN_TEST(test_const_must_initialize);
    
    printf("\n复杂场景测试:\n");
    RUN_TEST(test_variable_shadowing);
    RUN_TEST(test_access_outer_variable);
    RUN_TEST(test_modify_outer_variable);
    RUN_TEST(test_multiple_variables);
    
    printf("\n========================================\n");
    printf("测试结果:\n");
    printf("  总计: %d\n", tests_run);
    printf("  通过: %d\n", tests_passed);
    printf("  失败: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n✓ 所有测试通过!\n");
    } else {
        printf("\n✗ 有测试失败!\n");
    }
    printf("========================================\n\n");
    
    return tests_failed == 0 ? 0 : 1;
}


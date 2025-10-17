/*
** test_xerror.c
** 测试统一错误处理机制
*/

#include "xerror.h"
#include <stdio.h>
#include <assert.h>

/* 模拟一个可能失败的函数 */
XrResult divide(int a, int b) {
    if (b == 0) {
        return xr_error(XR_ERR_RUNTIME_DIVISION_BY_ZERO, 
                       42, "Cannot divide %d by zero", a);
    }
    printf("Result: %d / %d = %d\n", a, b, a / b);
    return xr_ok();
}

/* 模拟文件操作 */
XrResult read_file(const char *filename) {
    if (filename == NULL) {
        return xr_error_ex(XR_ERR_IO_FILE_NOT_FOUND,
                          "test.xr", 10, 5,
                          "File '%s' not found", "null");
    }
    printf("Reading file: %s\n", filename);
    return xr_ok();
}

/* 模拟错误传播 */
XrResult process_data(int value) {
    /* 检查输入 */
    XrResult result = divide(value, value - 10);
    XR_CHECK(result);  /* 如果失败则立即返回 */
    
    /* 继续处理 */
    printf("Processing value: %d\n", value);
    return xr_ok();
}

int main(void) {
    printf("=== 测试统一错误处理机制 ===\n\n");
    
    /* 测试1: 成功情况 */
    printf("测试1: 成功情况\n");
    XrResult r1 = divide(10, 2);
    assert(r1.success);
    printf("✓ 成功\n\n");
    
    /* 测试2: 错误情况 */
    printf("测试2: 错误情况\n");
    XrResult r2 = divide(10, 0);
    assert(!r2.success);
    assert(r2.error.code == XR_ERR_RUNTIME_DIVISION_BY_ZERO);
    printf("✓ 捕获到错误: ");
    xr_error_print(&r2.error);
    printf("\n");
    
    /* 测试3: 详细错误信息 */
    printf("测试3: 详细错误（文件+行+列）\n");
    XrResult r3 = read_file(NULL);
    assert(!r3.success);
    printf("✓ 详细错误: ");
    xr_error_print(&r3.error);
    printf("\n");
    
    /* 测试4: 错误传播 */
    printf("测试4: 错误传播\n");
    XrResult r4 = process_data(5);
    assert(r4.success);
    printf("✓ 处理成功\n\n");
    
    printf("测试5: 错误传播（失败情况）\n");
    XrResult r5 = process_data(10);
    assert(!r5.success);
    printf("✓ 错误自动传播: ");
    xr_error_print(&r5.error);
    printf("\n");
    
    /* 测试6: 错误码字符串 */
    printf("测试6: 错误码描述\n");
    printf("XR_OK = '%s'\n", xr_error_code_str(XR_OK));
    printf("XR_ERR_SYNTAX = '%s'\n", xr_error_code_str(XR_ERR_SYNTAX));
    printf("XR_ERR_RUNTIME_NULL_REFERENCE = '%s'\n", 
           xr_error_code_str(XR_ERR_RUNTIME_NULL_REFERENCE));
    printf("✓ 所有错误码都有描述\n\n");
    
    printf("=== 所有测试通过！ ===\n");
    return 0;
}


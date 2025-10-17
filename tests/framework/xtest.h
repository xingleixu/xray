/*
** xtest.h
** Xray 统一测试框架 - 极简单元测试
** 
** 职责：
**   - 提供统一的测试宏
**   - 统一的断言机制
**   - 清晰的测试报告
** 
** 设计灵感：minunit, Unity
** v0.13.7: 新增测试框架
*/

#ifndef xtest_h
#define xtest_h

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

/* ========== 颜色输出（可选）========== */

#ifdef XTEST_NO_COLOR
  #define XTEST_COLOR_RED     ""
  #define XTEST_COLOR_GREEN   ""
  #define XTEST_COLOR_YELLOW  ""
  #define XTEST_COLOR_BLUE    ""
  #define XTEST_COLOR_RESET   ""
#else
  #define XTEST_COLOR_RED     "\033[1;31m"
  #define XTEST_COLOR_GREEN   "\033[1;32m"
  #define XTEST_COLOR_YELLOW  "\033[1;33m"
  #define XTEST_COLOR_BLUE    "\033[1;34m"
  #define XTEST_COLOR_RESET   "\033[0m"
#endif

/* ========== 测试统计 ========== */

typedef struct {
    int total;         /* 总测试数 */
    int passed;        /* 通过数 */
    int failed;        /* 失败数 */
    const char *current_suite;  /* 当前测试套件名 */
    const char *current_test;   /* 当前测试名 */
} XTestStats;

static XTestStats xtest_stats = {0, 0, 0, NULL, NULL};

/* ========== 测试套件 ========== */

/*
** 开始测试套件
*/
#define XTEST_SUITE(name) \
    do { \
        xtest_stats.current_suite = name; \
        printf("\n"); \
        printf(XTEST_COLOR_BLUE "========================================\n"); \
        printf("   %s\n", name); \
        printf("========================================\n" XTEST_COLOR_RESET); \
        printf("\n"); \
    } while(0)

/*
** 测试分组（可选）
*/
#define XTEST_GROUP(name) \
    printf(XTEST_COLOR_YELLOW "=== %s ===\n" XTEST_COLOR_RESET, name)

/* ========== 断言宏 ========== */

/*
** 断言条件为真
*/
#define XTEST_ASSERT(condition, message) \
    do { \
        xtest_stats.total++; \
        if (condition) { \
            xtest_stats.passed++; \
            printf(XTEST_COLOR_GREEN "✓ 通过" XTEST_COLOR_RESET ": %s\n", message); \
        } else { \
            xtest_stats.failed++; \
            printf(XTEST_COLOR_RED "✗ 失败" XTEST_COLOR_RESET ": %s\n", message); \
            printf("  位置: %s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

/*
** 断言相等（整数）
*/
#define XTEST_ASSERT_EQ(expected, actual, message) \
    do { \
        xtest_stats.total++; \
        if ((expected) == (actual)) { \
            xtest_stats.passed++; \
            printf(XTEST_COLOR_GREEN "✓ 通过" XTEST_COLOR_RESET ": %s\n", message); \
        } else { \
            xtest_stats.failed++; \
            printf(XTEST_COLOR_RED "✗ 失败" XTEST_COLOR_RESET ": %s\n", message); \
            printf("  期望: %ld, 实际: %ld\n", (long)(expected), (long)(actual)); \
            printf("  位置: %s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

/*
** 断言不相等
*/
#define XTEST_ASSERT_NE(value1, value2, message) \
    do { \
        xtest_stats.total++; \
        if ((value1) != (value2)) { \
            xtest_stats.passed++; \
            printf(XTEST_COLOR_GREEN "✓ 通过" XTEST_COLOR_RESET ": %s\n", message); \
        } else { \
            xtest_stats.failed++; \
            printf(XTEST_COLOR_RED "✗ 失败" XTEST_COLOR_RESET ": %s\n", message); \
            printf("  两值相等: %ld\n", (long)(value1)); \
            printf("  位置: %s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

/*
** 断言为NULL
*/
#define XTEST_ASSERT_NULL(ptr, message) \
    XTEST_ASSERT((ptr) == NULL, message)

/*
** 断言非NULL
*/
#define XTEST_ASSERT_NOT_NULL(ptr, message) \
    XTEST_ASSERT((ptr) != NULL, message)

/*
** 断言字符串相等
*/
#define XTEST_ASSERT_STR_EQ(expected, actual, message) \
    do { \
        xtest_stats.total++; \
        if (strcmp((expected), (actual)) == 0) { \
            xtest_stats.passed++; \
            printf(XTEST_COLOR_GREEN "✓ 通过" XTEST_COLOR_RESET ": %s\n", message); \
        } else { \
            xtest_stats.failed++; \
            printf(XTEST_COLOR_RED "✗ 失败" XTEST_COLOR_RESET ": %s\n", message); \
            printf("  期望: \"%s\", 实际: \"%s\"\n", (expected), (actual)); \
            printf("  位置: %s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

/*
** 断言浮点数相等（带误差）
*/
#define XTEST_ASSERT_FLOAT_EQ(expected, actual, epsilon, message) \
    do { \
        xtest_stats.total++; \
        double _diff = (expected) - (actual); \
        if (_diff < 0) _diff = -_diff; \
        if (_diff < (epsilon)) { \
            xtest_stats.passed++; \
            printf(XTEST_COLOR_GREEN "✓ 通过" XTEST_COLOR_RESET ": %s\n", message); \
        } else { \
            xtest_stats.failed++; \
            printf(XTEST_COLOR_RED "✗ 失败" XTEST_COLOR_RESET ": %s\n", message); \
            printf("  期望: %.10f, 实际: %.10f, 差值: %.10f\n", \
                   (double)(expected), (double)(actual), _diff); \
            printf("  位置: %s:%d\n", __FILE__, __LINE__); \
        } \
    } while(0)

/* ========== 测试总结 ========== */

/*
** 打印测试总结并返回退出码
*/
#define XTEST_SUMMARY() \
    do { \
        printf("\n"); \
        printf("========================================\n"); \
        printf("   测试总结\n"); \
        printf("========================================\n"); \
        printf("总数: %d  ", xtest_stats.total); \
        printf(XTEST_COLOR_GREEN "通过: %d  " XTEST_COLOR_RESET, xtest_stats.passed); \
        printf(XTEST_COLOR_RED "失败: %d\n" XTEST_COLOR_RESET, xtest_stats.failed); \
        if (xtest_stats.failed == 0) { \
            printf(XTEST_COLOR_GREEN "✓ 所有测试通过！\n" XTEST_COLOR_RESET); \
        } else { \
            printf(XTEST_COLOR_RED "✗ 有 %d 个测试失败\n" XTEST_COLOR_RESET, xtest_stats.failed); \
        } \
        printf("========================================\n"); \
    } while(0)

/*
** 返回测试结果退出码
*/
#define XTEST_EXIT() \
    (xtest_stats.failed == 0 ? 0 : 1)

/* ========== 便捷宏组合 ========== */

/*
** 完整的测试程序模板
*/
#define XTEST_MAIN_BEGIN(suite_name) \
    int main(void) { \
        XTEST_SUITE(suite_name);

#define XTEST_MAIN_END() \
        XTEST_SUMMARY(); \
        return XTEST_EXIT(); \
    }

/* ========== 性能测试辅助 ========== */

#include <time.h>

/*
** 简单的性能计时
*/
#define XTEST_BENCHMARK(name, code) \
    do { \
        printf("\n[性能测试] %s\n", name); \
        clock_t _start = clock(); \
        code \
        clock_t _end = clock(); \
        double _time = (double)(_end - _start) / CLOCKS_PER_SEC; \
        printf("  耗时: %.6f 秒\n", _time); \
    } while(0)

#endif /* xtest_h */


/*
** xerror.h
** Xray 统一错误处理机制
** 
** 职责：
**   - 定义统一的错误码和错误类型
**   - 提供错误信息管理
**   - 统一错误报告格式
** 
** v0.13.6: 新增统一错误处理
*/

#ifndef xerror_h
#define xerror_h

#include <stdbool.h>
#include <stdarg.h>

/* ========== 错误码定义 ========== */

typedef enum {
    XR_OK = 0,                  /* 成功，无错误 */
    
    /* 词法错误 (1-99) */
    XR_ERR_LEXER_INVALID_CHAR = 1,
    XR_ERR_LEXER_UNTERMINATED_STRING,
    XR_ERR_LEXER_INVALID_NUMBER,
    
    /* 语法错误 (100-199) */
    XR_ERR_SYNTAX = 100,
    XR_ERR_SYNTAX_UNEXPECTED_TOKEN,
    XR_ERR_SYNTAX_EXPECT_EXPRESSION,
    XR_ERR_SYNTAX_EXPECT_SEMICOLON,
    XR_ERR_SYNTAX_EXPECT_RPAREN,
    XR_ERR_SYNTAX_EXPECT_RBRACE,
    XR_ERR_SYNTAX_EXPECT_RBRACKET,
    XR_ERR_SYNTAX_INVALID_ASSIGNMENT,
    
    /* 编译错误 (200-299) */
    XR_ERR_COMPILE = 200,
    XR_ERR_COMPILE_TOO_MANY_LOCALS,
    XR_ERR_COMPILE_TOO_MANY_CONSTANTS,
    XR_ERR_COMPILE_TOO_MANY_UPVALUES,
    XR_ERR_COMPILE_VARIABLE_REDEFINED,
    XR_ERR_COMPILE_UNDEFINED_VARIABLE,
    XR_ERR_COMPILE_JUMP_TOO_LARGE,
    
    /* 类型错误 (300-399) */
    XR_ERR_TYPE = 300,
    XR_ERR_TYPE_MISMATCH,
    XR_ERR_TYPE_NOT_CALLABLE,
    XR_ERR_TYPE_NOT_INDEXABLE,
    XR_ERR_TYPE_NOT_ITERABLE,
    XR_ERR_TYPE_INVALID_OPERAND,
    
    /* 运行时错误 (400-499) */
    XR_ERR_RUNTIME = 400,
    XR_ERR_RUNTIME_STACK_OVERFLOW,
    XR_ERR_RUNTIME_STACK_UNDERFLOW,
    XR_ERR_RUNTIME_DIVISION_BY_ZERO,
    XR_ERR_RUNTIME_INDEX_OUT_OF_BOUNDS,
    XR_ERR_RUNTIME_NULL_REFERENCE,
    XR_ERR_RUNTIME_INVALID_OPERATION,
    XR_ERR_RUNTIME_UNDEFINED_PROPERTY,
    XR_ERR_RUNTIME_UNDEFINED_METHOD,
    
    /* 内存错误 (500-599) */
    XR_ERR_MEMORY = 500,
    XR_ERR_MEMORY_ALLOCATION_FAILED,
    XR_ERR_MEMORY_OUT_OF_MEMORY,
    
    /* IO错误 (600-699) */
    XR_ERR_IO = 600,
    XR_ERR_IO_FILE_NOT_FOUND,
    XR_ERR_IO_READ_FAILED,
    XR_ERR_IO_WRITE_FAILED,
    
    /* 其他错误 */
    XR_ERR_INTERNAL = 900,
    XR_ERR_NOT_IMPLEMENTED = 901,
    XR_ERR_UNKNOWN = 999
} XrErrorCode;

/* ========== 错误信息结构 ========== */

#define XR_ERROR_MSG_MAX 256

typedef struct {
    XrErrorCode code;           /* 错误码 */
    int line;                   /* 错误所在行号 */
    int column;                 /* 错误所在列号 */
    char message[XR_ERROR_MSG_MAX];  /* 错误消息 */
    const char *file;           /* 错误所在文件（可选）*/
} XrError;

/* ========== 错误结果类型 ========== */

typedef struct {
    bool success;               /* 是否成功 */
    XrError error;              /* 错误信息（当success=false时有效）*/
} XrResult;

/* ========== 错误处理函数 ========== */

/*
** 创建成功结果
*/
XrResult xr_ok(void);

/*
** 创建错误结果
** @param code 错误码
** @param line 行号
** @param format 格式化消息
*/
XrResult xr_error(XrErrorCode code, int line, const char *format, ...);

/*
** 创建详细错误结果（包含文件和列信息）
*/
XrResult xr_error_ex(XrErrorCode code, const char *file, int line, 
                     int column, const char *format, ...);

/*
** 打印错误信息到stderr
*/
void xr_error_print(const XrError *error);

/*
** 获取错误码的字符串描述
*/
const char* xr_error_code_str(XrErrorCode code);

/* ========== 便捷宏 ========== */

/* 检查结果，如果失败则返回 */
#define XR_CHECK(result) \
    do { \
        XrResult _r = (result); \
        if (!_r.success) return _r; \
    } while(0)

/* 检查结果，如果失败则跳转到cleanup */
#define XR_CHECK_GOTO(result, label) \
    do { \
        XrResult _r = (result); \
        if (!_r.success) { \
            _error = _r; \
            goto label; \
        } \
    } while(0)

/* 断言错误码为OK */
#define XR_ASSERT_OK(result) \
    do { \
        XrResult _r = (result); \
        if (!_r.success) { \
            xr_error_print(&_r.error); \
            abort(); \
        } \
    } while(0)

/* ========== 向后兼容宏 ========== */

/* 用于逐步迁移现有代码 */
#define XR_ERROR_LEGACY(msg) \
    xr_error(XR_ERR_RUNTIME, -1, "%s", msg)

#endif /* xerror_h */


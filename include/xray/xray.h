/*
** xray.h
** Xray 脚本语言核心头文件
** 定义了语言的基本接口和核心数据结构
*/

#ifndef xray_h
#define xray_h

#include <stddef.h>
#include <stdint.h>

/* 版本信息 */
#define XRAY_VERSION_MAJOR  0
#define XRAY_VERSION_MINOR  4
#define XRAY_VERSION_PATCH  0
#define XRAY_VERSION        "Xray 0.4.0"
#define XRAY_COPYRIGHT      "Copyright (C) 2025"
#define XRAY_AUTHORS        "Xray Team"

/* 基本类型定义 */
typedef struct XrayState XrayState;
typedef double xr_Number;
typedef int64_t xr_Integer;

/* API 函数（已废弃 - 仅保留基本类型定义） */
/* 
 * 注意：以下函数已废弃，请使用新的API：
 * - xray_newstate() -> xr_state_new()  (在xstate.h)
 * - xray_close()    -> xr_state_free() (在xstate.h)
 * 
 * 这些函数定义保留在xstate.c中作为向后兼容包装
 */
XrayState *xray_newstate(void);  /* 废弃 */
void xray_close(XrayState *X);   /* 废弃 */
int xray_dofile(XrayState *X, const char *filename);
int xray_dostring(XrayState *X, const char *source);

#endif /* xray_h */


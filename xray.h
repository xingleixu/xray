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
#define XRAY_VERSION_MINOR  2
#define XRAY_VERSION_PATCH  0
#define XRAY_VERSION        "Xray 0.2.0"
#define XRAY_COPYRIGHT      "Copyright (C) 2025"
#define XRAY_AUTHORS        "Xray Team"

/* 基本类型定义 */
typedef struct XrayState XrayState;
typedef double xr_Number;
typedef int64_t xr_Integer;

/* API 函数 */
XrayState *xray_newstate(void);
void xray_close(XrayState *X);
int xray_dofile(XrayState *X, const char *filename);
int xray_dostring(XrayState *X, const char *source);

#endif /* xray_h */


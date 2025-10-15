/*
** xdebug.h
** Xray 字节码调试工具 - 反汇编器
** 
** v0.13.0: 字节码反汇编和调试支持
** 创建日期: 2025-10-15
*/

#ifndef xdebug_h
#define xdebug_h

#include "xchunk.h"

/* ========== 反汇编API ========== */

/*
** 反汇编整个函数原型
** @param proto 函数原型
** @param name 函数名（用于显示）
*/
void xr_disassemble_proto(Proto *proto, const char *name);

/*
** 反汇编单条指令
** @param proto 函数原型
** @param offset 指令偏移
** @return 下一条指令的偏移
*/
int xr_disassemble_instruction(Proto *proto, int offset);

/* ========== 调试输出 ========== */

/*
** 打印常量值
** @param value 要打印的值
*/
void xr_print_value(XrValue value);

/*
** 打印常量表
** @param proto 函数原型
*/
void xr_print_constants(Proto *proto);

#endif /* xdebug_h */


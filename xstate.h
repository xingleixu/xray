/*
** xstate.h
** Xray 状态管理
** 定义和管理 Xray 虚拟机的全局状态
*/

#ifndef xstate_h
#define xstate_h

#include "xray.h"

/*
** Xray 状态结构
** 包含虚拟机运行所需的所有状态信息
*/
struct XrayState {
    /* 暂时只包含基本字段，后续会扩展 */
    void *userdata;     /* 用户数据指针 */
};

#endif /* xstate_h */


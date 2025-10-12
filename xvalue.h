/*
** xvalue.h
** Xray 值类型系统定义
** 定义了语言支持的所有数据类型
*/

#ifndef xvalue_h
#define xvalue_h

#include "xray.h"

/* 
** 类型标签
** Xray 支持的基本类型：null、bool、int、float、string、array、set、map、class
*/
typedef enum {
    XR_TNULL,       /* null 类型 */
    XR_TBOOL,       /* 布尔类型 */
    XR_TINT,        /* 整数类型 */
    XR_TFLOAT,      /* 浮点数类型 */
    XR_TSTRING,     /* 字符串类型 */
    XR_TARRAY,      /* 数组类型 */
    XR_TSET,        /* 集合类型 */
    XR_TMAP,        /* 映射类型 */
    XR_TCLASS       /* 类类型 */
} XrType;

/* 
** 值对象
** 使用带标签的联合体实现多态值
*/
typedef struct {
    XrType type;
    union {
        int b;           /* 布尔值 */
        xr_Integer i;    /* 整数值 */
        xr_Number n;     /* 浮点数值 */
        void *obj;       /* 对象指针（字符串、数组等） */
    } as;
} XrValue;

/* 类型检查宏 */
#define xr_isnull(v)    ((v)->type == XR_TNULL)
#define xr_isbool(v)    ((v)->type == XR_TBOOL)
#define xr_isint(v)     ((v)->type == XR_TINT)
#define xr_isfloat(v)   ((v)->type == XR_TFLOAT)
#define xr_isstring(v)  ((v)->type == XR_TSTRING)
#define xr_isarray(v)   ((v)->type == XR_TARRAY)

/* 值访问宏 */
#define xr_tobool(v)    ((v)->as.b)
#define xr_toint(v)     ((v)->as.i)
#define xr_tofloat(v)   ((v)->as.n)
#define xr_toobj(v)     ((v)->as.obj)

/* 值设置宏 */
#define xr_setnull(v)       ((v)->type = XR_TNULL)
#define xr_setbool(v, val)  ((v)->type = XR_TBOOL, (v)->as.b = (val))
#define xr_setint(v, val)   ((v)->type = XR_TINT, (v)->as.i = (val))
#define xr_setfloat(v, val) ((v)->type = XR_TFLOAT, (v)->as.n = (val))

/* 值操作函数 */
const char *xr_typename(XrType type);

#endif /* xvalue_h */


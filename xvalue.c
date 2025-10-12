/*
** xvalue.c
** Xray 值类型系统实现
*/

#include "xvalue.h"

/* 
** 返回类型名称字符串
** 用于调试和错误信息
*/
const char *xr_typename(XrType type) {
    switch (type) {
        case XR_TNULL:   return "null";
        case XR_TBOOL:   return "bool";
        case XR_TINT:    return "int";
        case XR_TFLOAT:  return "float";
        case XR_TSTRING: return "string";
        case XR_TARRAY:  return "array";
        case XR_TSET:    return "set";
        case XR_TMAP:    return "map";
        case XR_TCLASS:  return "class";
        default:         return "unknown";
    }
}


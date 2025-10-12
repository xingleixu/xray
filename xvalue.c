/*
** xvalue.c
** Xray 值类型系统实现
*/

#include "xvalue.h"
#include <stdlib.h>
#include <string.h>

/* 
** 返回类型名称字符串
** 用于调试和错误信息
*/
const char *xr_typename(XrType type) {
    switch (type) {
        case XR_TNULL:     return "null";
        case XR_TBOOL:     return "bool";
        case XR_TINT:      return "int";
        case XR_TFLOAT:    return "float";
        case XR_TSTRING:   return "string";
        case XR_TFUNCTION: return "function";
        case XR_TARRAY:    return "array";
        case XR_TSET:      return "set";
        case XR_TMAP:      return "map";
        case XR_TCLASS:    return "class";
        default:           return "unknown";
    }
}

/*
** 创建新的函数对象
** name: 函数名
** parameters: 参数列表（字符串数组）
** param_count: 参数数量
** body: 函数体（AST节点）
*/
XrFunction *xr_function_new(const char *name, char **parameters,
                            int param_count, AstNode *body) {
    XrFunction *func = (XrFunction *)malloc(sizeof(XrFunction));
    if (func == NULL) {
        return NULL;
    }
    
    /* 复制函数名 */
    func->name = (char *)malloc(strlen(name) + 1);
    strcpy(func->name, name);
    
    /* 复制参数列表 */
    func->param_count = param_count;
    if (param_count > 0) {
        func->parameters = (char **)malloc(sizeof(char *) * param_count);
        for (int i = 0; i < param_count; i++) {
            func->parameters[i] = (char *)malloc(strlen(parameters[i]) + 1);
            strcpy(func->parameters[i], parameters[i]);
        }
    } else {
        func->parameters = NULL;
    }
    
    /* 存储函数体和闭包作用域 */
    func->body = body;
    func->closure_scope = NULL;  /* 第六阶段实现闭包时会使用 */
    
    return func;
}

/*
** 释放函数对象
** func: 要释放的函数对象
*/
void xr_function_free(XrFunction *func) {
    if (func == NULL) {
        return;
    }
    
    /* 释放函数名 */
    if (func->name != NULL) {
        free(func->name);
    }
    
    /* 释放参数列表 */
    if (func->parameters != NULL) {
        for (int i = 0; i < func->param_count; i++) {
            free(func->parameters[i]);
        }
        free(func->parameters);
    }
    
    /* 注意：func->body 不在这里释放，因为它是 AST 的一部分 */
    /* AST 会在 xr_ast_free 中统一释放 */
    
    /* 释放函数对象本身 */
    free(func);
}


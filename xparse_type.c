/*
** xparse_type.c
** Xray 类型解析器
** 
** 解析类型注解：int, float, string, int[], int | string, int?等
*/

#include "xparse.h"
#include "xtype.h"
#include <stdlib.h>
#include <string.h>

/* ========== 类型解析函数 ========== */

/*
** 解析基础类型
** 如：int, float, string, bool, void, any
*/
static XrTypeInfo* parse_primary_type(Parser *p) {
    if (match(p, TK_TYPE_INT)) {
        return xr_type_int(p->X);
    }
    if (match(p, TK_TYPE_FLOAT)) {
        return xr_type_float(p->X);
    }
    if (match(p, TK_TYPE_STRING)) {
        return xr_type_string(p->X);
    }
    if (match(p, TK_BOOL)) {
        return xr_type_bool(p->X);
    }
    if (match(p, TK_VOID)) {
        return xr_type_void(p->X);
    }
    if (match(p, TK_ANY)) {
        return xr_type_any(p->X);
    }
    
    error(p, "期望类型名称");
    return xr_type_any(p->X);  /* 错误恢复：返回any */
}

/*
** 解析数组类型
** 如：int[], int[][], string[]
*/
static XrTypeInfo* parse_array_type(Parser *p) {
    XrTypeInfo *base = parse_primary_type(p);
    
    /* 处理多维数组：int[][] */
    while (match(p, TK_LBRACKET)) {
        consume(p, TK_RBRACKET, "期望 ']'");
        base = xr_type_array(p->X, base);
    }
    
    return base;
}

/*
** 解析联合类型
** 如：int | string, int | float | null
*/
static XrTypeInfo* parse_union_type(Parser *p) {
    XrTypeInfo *base = parse_array_type(p);
    
    /* 检查是否有 | */
    if (match(p, TK_PIPE)) {
        /* 收集所有联合成员 */
        #define MAX_UNION_TYPES 16
        XrTypeInfo *types[MAX_UNION_TYPES];
        int count = 0;
        
        /* 第一个类型 */
        types[count++] = base;
        
        /* 后续类型 */
        do {
            if (count >= MAX_UNION_TYPES) {
                error(p, "联合类型成员过多（最多16个）");
                break;
            }
            types[count++] = parse_array_type(p);
        } while (match(p, TK_PIPE));
        
        /* 创建联合类型 */
        return xr_type_union(p->X, types, count);
    }
    
    return base;
}

/*
** 解析类型（主入口）
** 如：int, int[], int | string, int?
*/
XrTypeInfo* xr_parse_type(Parser *p) {
    XrTypeInfo *base = parse_union_type(p);
    
    /* 可选类型：int? */
    if (match(p, TK_QUESTION)) {
        return xr_type_optional(p->X, base);
    }
    
    return base;
}


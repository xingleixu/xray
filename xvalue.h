/*
** xvalue.h
** Xray 值对象系统 - 双模式设计
** 
** v0.6.0：支持类型信息，预留NaN Tagging接口
** 参考：Wren的双模式设计
*/

#ifndef xvalue_h
#define xvalue_h

#include "xray.h"
#include <stdint.h>
#include <stdbool.h>

/* 前向声明 */
typedef struct AstNode AstNode;
typedef struct XScope XScope;
typedef struct XrTypeInfo XrTypeInfo;
typedef struct XrString XrString;

/*
** 编译时开关：NaN Tagging模式
** 0 = Tagged Union（调试模式，16字节）- 第6阶段
** 1 = NaN Tagging（性能模式，8字节）- 第13阶段
*/
#ifndef XR_NAN_TAGGING
  #define XR_NAN_TAGGING 1  /* ⚡ 启用NaN Tagging优化！*/
#endif

/* 
** 类型标签
** 基本类型标识符
*/
typedef enum {
    XR_TNULL,       /* null 类型 */
    XR_TBOOL,       /* 布尔类型 */
    XR_TINT,        /* 整数类型 */
    XR_TFLOAT,      /* 浮点数类型 */
    XR_TSTRING,     /* 字符串类型 */
    XR_TFUNCTION,   /* 函数类型（Xray闭包） */
    XR_TCFUNCTION,  /* C函数类型（v0.14.1新增）*/
    XR_TARRAY,      /* 数组类型 */
    XR_TSET,        /* 集合类型 */
    XR_TMAP,        /* 映射类型 */
    XR_TCLASS,      /* 类类型（v0.12.0新增）*/
    XR_TINSTANCE    /* 实例类型（v0.12.0新增）*/
} XrType;

/*
** 对象头部（用于GC和类型信息）
** 所有堆分配对象的公共头部
*/
typedef struct XrObject {
    struct XrObject *next;   /* GC链表指针 */
    XrType type;             /* 对象基本类型 */
    XrTypeInfo *type_info;   /* 详细类型信息 */
    bool marked;             /* GC标记位 */
} XrObject;

/* ========== 值表示（双模式）========== */

#if XR_NAN_TAGGING

  /* ========== 性能模式（第13阶段启用）========== */
  typedef uint64_t XrValue;
  
  /* NaN Tagging宏定义 */
  #define XR_SIGN_BIT ((uint64_t)1 << 63)
  #define XR_QNAN     ((uint64_t)0x7ffc000000000000)
  
  /* SMI (Small Integer)编码：使用LSB标记整数 */
  #define XR_IS_SMI(v)      (((v) & 1) == 0)
  #define XR_SMI_TO_INT(v)  ((xr_Integer)((int64_t)(v) >> 1))
  #define XR_INT_TO_SMI(i)  ((XrValue)((uint64_t)((int64_t)(i) << 1)))
  #define XR_SMI_MIN        (-(1LL << 47))
  #define XR_SMI_MAX        ((1LL << 47) - 1)
  
  /* 类型判断宏 */
  #define XR_IS_NUM(v)    (XR_IS_SMI(v) || (((v) & XR_QNAN) != XR_QNAN))
  #define XR_IS_INT(v)    (XR_IS_SMI(v))
  #define XR_IS_FLOAT(v)  (!XR_IS_SMI(v) && (((v) & XR_QNAN) != XR_QNAN))
  #define XR_IS_OBJ(v)    (((v) & (XR_QNAN | XR_SIGN_BIT)) == (XR_QNAN | XR_SIGN_BIT))
  #define XR_IS_NULL(v)   ((v) == XR_NULL_VAL)
  #define XR_IS_BOOL(v)   (((v) & ~1ULL) == XR_FALSE_VAL)
  #define XR_IS_STRING(v) (XR_IS_OBJ(v) && ((XrObject*)XR_TO_OBJ(v))->type == XR_TSTRING)
  #define XR_IS_FUNCTION(v) (XR_IS_OBJ(v) && ((XrObject*)XR_TO_OBJ(v))->type == XR_TFUNCTION)
  #define XR_IS_ARRAY(v) (XR_IS_OBJ(v) && ((XrObject*)XR_TO_OBJ(v))->type == XR_TARRAY)
  
  /* 值访问宏 */
  #define XR_TO_BOOL(v)   ((int)((v) == XR_TRUE_VAL))
  #define XR_TO_INT(v)    (XR_SMI_TO_INT(v))
  #define XR_TO_FLOAT(v)  (wrenDoubleFromBits(v))
  #define XR_TO_OBJ(v)    ((void*)(uintptr_t)((v) & ~(XR_SIGN_BIT | XR_QNAN)))
  
  /* 单例值（预留） */
  #define XR_NULL_VAL     ((XrValue)(uint64_t)(XR_QNAN | 1))
  #define XR_FALSE_VAL    ((XrValue)(uint64_t)(XR_QNAN | 2))
  #define XR_TRUE_VAL     ((XrValue)(uint64_t)(XR_QNAN | 3))
  
  /* 双精度浮点数编码/解码（第13阶段实现） */
  /* 
  ** Union用于在double和uint64_t之间进行类型双关转换
  ** 这是标准的NaN Tagging技巧，被Wren, LuaJIT等广泛使用
  */
  typedef union {
    uint64_t bits64;
    double num;
  } XrDoubleBits;
  
  static inline double wrenDoubleFromBits(uint64_t bits) {
    XrDoubleBits data;
    data.bits64 = bits;
    return data.num;
  }
  
  static inline uint64_t wrenDoubleToBits(double num) {
    XrDoubleBits data;
    data.num = num;
    return data.bits64;
  }
  
  /* 对象转值宏（对象指针 -> NaN Tagged Value） */
  #define XR_OBJ_TO_VAL(obj) \
    ((XrValue)(XR_SIGN_BIT | XR_QNAN | (uint64_t)(uintptr_t)(obj)))

#else

  /* ========== 调试模式（第6阶段使用）========== */
  typedef struct {
    XrType type;              /* 基本类型标签 */
    XrTypeInfo *type_info;    /* 详细类型信息（新增！）*/
    union {
      int b;                  /* 布尔值 */
      xr_Integer i;           /* 整数值 */
      xr_Number n;            /* 浮点数值 */
      void *obj;              /* 对象指针 */
    } as;
  } XrValue;
  
  /* 类型判断宏（调试模式实现） */
  #define XR_IS_NULL(v)   ((v).type == XR_TNULL)
  #define XR_IS_BOOL(v)   ((v).type == XR_TBOOL)
  #define XR_IS_INT(v)    ((v).type == XR_TINT)
  #define XR_IS_FLOAT(v)  ((v).type == XR_TFLOAT)
  #define XR_IS_STRING(v) ((v).type == XR_TSTRING)
  #define XR_IS_FUNCTION(v) ((v).type == XR_TFUNCTION)
  #define XR_IS_ARRAY(v)  ((v).type == XR_TARRAY)
  #define XR_IS_OBJ(v)    ((v).type >= XR_TFUNCTION)
  
  /* 值访问宏（调试模式实现） */
  #define XR_TO_BOOL(v)   ((v).as.b)
  #define XR_TO_INT(v)    ((v).as.i)
  #define XR_TO_FLOAT(v)  ((v).as.n)
  #define XR_TO_OBJ(v)    ((v).as.obj)

#endif

/* ========== 统一API（支持双模式切换）========== */

/* 类型检查宏（外部使用这些） */
#define xr_isnull(v)      XR_IS_NULL(v)
#define xr_isbool(v)      XR_IS_BOOL(v)
#define xr_isint(v)       XR_IS_INT(v)
#define xr_isfloat(v)     XR_IS_FLOAT(v)
#define xr_isstring(v)    XR_IS_STRING(v)
#define xr_isfunction(v)  XR_IS_FUNCTION(v)
#define xr_isarray(v)     XR_IS_ARRAY(v)

/* 值访问宏（外部使用这些） */
#define xr_tobool(v)      XR_TO_BOOL(v)
#define xr_toint(v)       XR_TO_INT(v)
#define xr_tofloat(v)     XR_TO_FLOAT(v)
#define xr_toobj(v)       XR_TO_OBJ(v)
#define xr_tostring(v)    ((XrString*)XR_TO_OBJ(v))
#define xr_tofunction(v)  ((XrFunction*)XR_TO_OBJ(v))

/* ========== 值创建函数（新API⭐）========== */

/* 基本值创建（使用内置类型） */
XrValue xr_null(void);
XrValue xr_bool(int b);
XrValue xr_int(xr_Integer i);
XrValue xr_float(xr_Number n);

/* 带类型信息的值创建 */
XrValue xr_make_int(xr_Integer i, XrTypeInfo *type_info);
XrValue xr_make_float(xr_Number n, XrTypeInfo *type_info);
XrValue xr_make_bool(int b, XrTypeInfo *type_info);

/* ========== 类型信息访问（新增）========== */

/* 获取值的类型信息 */
XrTypeInfo* xr_typeof(const XrValue *v);

/* 获取类型名称字符串 */
const char* xr_typename_str(const XrValue *v);

/* 类型检查 */
bool xr_value_is_type(const XrValue *v, XrTypeInfo *expected);

/* 基本类型名称（保留兼容） */
const char* xr_typename(XrType type);

/* 获取值的基本类型（新增 v0.13.4） */
XrType xr_value_type(XrValue v);

/* ========== 对象定义（在XrValue之后）========== */

/*
** 字符串对象（已移至 xstring.h）
*/
/* XrString 定义移至 xstring.h */

/*
** 数组对象（为第9阶段准备）
*/
/* XrArray 定义移至 xarray.h */

/*
** 函数对象
** 
** 注意：当前是树遍历解释器阶段的简化版本
** 第13阶段切换到字节码VM时，将使用完整的XrClosure+XrFnProto模型
*/
typedef struct {
    XrObject header;
    char *name;              /* 函数名 */
    char **parameters;       /* 参数列表 */
    XrTypeInfo **param_types;  /* 参数类型列表（新增） */
    int param_count;         /* 参数数量 */
    XrTypeInfo *return_type;   /* 返回类型（新增） */
    AstNode *body;           /* 函数体（AST节点） */
    XScope *closure_scope;   /* 闭包作用域（简化版，捕获外部变量） */
    
    /* ===== 第八阶段新增：闭包支持（简化版） ===== */
    char **captured_vars;    /* 捕获的变量名列表 */
    XrValue *captured_values; /* 捕获的变量值列表 */
    int captured_count;      /* 捕获的变量数量 */
    
    /* 预留：第13阶段将使用XrClosure+XrUpvalue模型 */
} XrFunction;

/* ========== 对象操作 ========== */

/* 字符串值创建（新增 v0.10.0）*/
XrValue xr_string_value(XrString *str);

/* 函数对象 */
XrFunction* xr_function_new(const char *name, char **parameters, 
                            XrTypeInfo **param_types, int param_count,
                            XrTypeInfo *return_type, AstNode *body);
void xr_function_free(XrFunction *func);

/* 函数值创建（新增 v0.13.4）*/
XrValue xr_function_value(XrFunction *func);

/* 闭包值操作 */
struct XrClosure;  /* 前向声明 */
XrValue xr_value_from_closure(struct XrClosure *closure);
bool xr_value_is_closure(XrValue v);
struct XrClosure* xr_value_to_closure(XrValue v);

/* C函数值操作（v0.14.1新增）*/
struct XrCFunction;  /* 前向声明 */
XrValue xr_value_from_cfunction(struct XrCFunction *cfunc);
bool xr_value_is_cfunction(XrValue v);
struct XrCFunction* xr_value_to_cfunction(XrValue v);

/* 数组值操作 */
struct XrArray;  /* 前向声明 */
XrValue xr_value_from_array(struct XrArray *arr);
bool xr_value_is_array(XrValue v);
struct XrArray* xr_value_to_array(XrValue v);

/* Map值操作（v0.11.0）*/
struct XrMap;    /* 前向声明 */
XrValue xr_value_from_map(struct XrMap *map);
bool xr_value_is_map(XrValue v);
struct XrMap* xr_value_to_map(XrValue v);

/* OOP值操作（v0.12.0）*/
struct XrClass;     /* 前向声明 */
struct XrInstance;  /* 前向声明 */
XrValue xr_value_from_class(struct XrClass *cls);
bool xr_value_is_class(XrValue v);
struct XrClass* xr_value_to_class(XrValue v);

XrValue xr_value_from_instance(struct XrInstance *inst);
bool xr_value_is_instance(XrValue v);
struct XrInstance* xr_value_to_instance(XrValue v);

/* 便捷宏 */
#define xr_is_array(v)    xr_value_is_array(v)
#define xr_to_array(v)    xr_value_to_array(v)
#define xr_is_map(v)      xr_value_is_map(v)
#define xr_to_map(v)      xr_value_to_map(v)
#define xr_is_class(v)    xr_value_is_class(v)
#define xr_to_class(v)    xr_value_to_class(v)
#define xr_is_instance(v) xr_value_is_instance(v)
#define xr_to_instance(v) xr_value_to_instance(v)

/* 对象头部操作 */
void xr_object_init(XrObject *obj, XrType type, XrTypeInfo *type_info);

#endif /* xvalue_h */

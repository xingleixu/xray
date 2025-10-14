/* xarray.h - Xray 动态数组对象
 *
 * 实现类似 JavaScript/TypeScript 的数组功能：
 * - 动态扩容
 * - 索引访问
 * - 基础方法：push, pop, unshift, shift
 * - 高阶方法：map, filter, forEach, reduce
 * 
 * 参考：
 * - Wren: ObjList（简洁清晰）
 * - Lua: Table 的数组部分（扩容策略）
 */

#ifndef XRAY_ARRAY_H
#define XRAY_ARRAY_H

#include "xray.h"
#include "xvalue.h"
#include "xobject.h"

/* 前向声明 */
struct XSymbolTable;

/* 数组初始容量 */
#define XR_ARRAY_INIT_CAPACITY 8

/* 数组对象结构
 * 
 * 内存布局：
 * +----------------+
 * | XrObjectHeader | (GC 头，预留)
 * +----------------+
 * | elements       | (指向动态分配的元素数组)
 * | count          | (当前元素数量)
 * | capacity       | (当前容量)
 * | element_type   | (元素类型信息，可选)
 * +----------------+
 * 
 * 扩容策略：
 * - 初始容量：8
 * - 扩容倍数：2x
 * - 不收缩（简化实现）
 */
typedef struct {
    XrObject header;            // GC对象头（预留）
    
    // 数组数据
    size_t capacity;            // 当前容量
    size_t count;               // 当前元素数量
    XrValue *elements;          // 元素数组（动态分配）
    
    // 类型信息（可选，用于类型检查）
    XrTypeInfo *element_type;   // 元素类型
} XrArray;

/* ====== 创建和销毁 ====== */

/**
 * 创建空数组
 * @return 新的数组对象
 */
XrArray* xr_array_new(void);

/**
 * 创建指定容量的数组（避免多次扩容）
 * @param capacity 初始容量
 * @return 新的数组对象
 */
XrArray* xr_array_with_capacity(int capacity);

/**
 * 创建并填充数组
 * @param elements 元素数组
 * @param count 元素数量
 * @return 新的数组对象
 */
XrArray* xr_array_from_values(XrValue *elements, int count);

/**
 * 释放数组内存
 * @param arr 要释放的数组
 */
void xr_array_free(XrArray *arr);

/* ====== 访问元素 ====== */

/**
 * 获取数组元素
 * @param arr 数组
 * @param index 索引（0-based）
 * @return 元素值
 * @note 如果索引越界，返回 null 值
 */
XrValue xr_array_get(XrArray *arr, int index);

/**
 * 设置数组元素
 * @param arr 数组
 * @param index 索引（0-based）
 * @param value 新值
 * @note 如果索引越界，不做任何操作
 */
void xr_array_set(XrArray *arr, int index, XrValue value);

/**
 * 获取数组长度
 * @param arr 数组
 * @return 元素数量
 */
int xr_array_length(XrArray *arr);

/* ====== 修改数组 ====== */

/**
 * 在数组末尾添加元素
 * @param arr 数组
 * @param value 要添加的值
 * @note 如果容量不足，自动扩容
 */
void xr_array_push(XrArray *arr, XrValue value);

/**
 * 移除并返回数组最后一个元素
 * @param arr 数组
 * @return 被移除的元素，如果数组为空返回 null
 */
XrValue xr_array_pop(XrArray *arr);

/**
 * 在数组开头添加元素
 * @param arr 数组
 * @param value 要添加的值
 * @note 需要移动所有现有元素，O(n) 操作
 */
void xr_array_unshift(XrArray *arr, XrValue value);

/**
 * 移除并返回数组第一个元素
 * @param arr 数组
 * @return 被移除的元素，如果数组为空返回 null
 * @note 需要移动所有剩余元素，O(n) 操作
 */
XrValue xr_array_shift(XrArray *arr);

/**
 * 清空数组
 * @param arr 数组
 * @note 不释放内存，只重置 count
 */
void xr_array_clear(XrArray *arr);

/* ====== 查询方法 ====== */

/**
 * 查找元素第一次出现的位置
 * @param arr 数组
 * @param value 要查找的值
 * @return 索引，如果不存在返回 -1
 */
int xr_array_index_of(XrArray *arr, XrValue value);

/**
 * 检查数组是否包含某个元素
 * @param arr 数组
 * @param value 要查找的值
 * @return true 如果存在，否则 false
 */
bool xr_array_contains(XrArray *arr, XrValue value);

/**
 * 检查数组是否为空
 * @param arr 数组
 * @return true 如果为空，否则 false
 */
bool xr_array_is_empty(XrArray *arr);

/* ====== 高阶方法 ====== */

/**
 * 遍历数组，对每个元素执行回调函数
 * @param arr 数组
 * @param callback 回调函数 (element, index?) => void
 * @param symbols 符号表（用于函数调用）
 * @note callback 可以接受 1 或 2 个参数
 */
void xr_array_foreach(XrArray *arr, XrFunction *callback, struct XSymbolTable *symbols);

/**
 * 映射数组，创建新数组
 * @param arr 数组
 * @param callback 回调函数 (element, index?) => value
 * @param symbols 符号表（用于函数调用）
 * @return 新的数组
 */
XrArray* xr_array_map(XrArray *arr, XrFunction *callback, struct XSymbolTable *symbols);

/**
 * 过滤数组，创建新数组
 * @param arr 数组
 * @param callback 回调函数 (element) => bool
 * @param symbols 符号表（用于函数调用）
 * @return 新的数组（只包含回调返回 true 的元素）
 */
XrArray* xr_array_filter(XrArray *arr, XrFunction *callback, struct XSymbolTable *symbols);

/**
 * 归约数组为单个值
 * @param arr 数组
 * @param callback 回调函数 (accumulator, element) => value
 * @param initial 初始值
 * @param symbols 符号表（用于函数调用）
 * @return 最终的累积值
 */
XrValue xr_array_reduce(XrArray *arr, XrFunction *callback, XrValue initial, struct XSymbolTable *symbols);

/* ====== 工具方法 ====== */

/**
 * 反转数组（原地修改）
 * @param arr 数组
 */
void xr_array_reverse(XrArray *arr);

/**
 * 复制数组
 * @param arr 源数组
 * @return 新的数组（深拷贝元素）
 */
XrArray* xr_array_copy(XrArray *arr);

/**
 * 打印数组内容（调试用）
 * @param arr 数组
 */
void xr_array_print(XrArray *arr);

/* ====== 内部函数 ====== */

/**
 * 数组扩容（内部函数）
 * @param arr 数组
 * @note 容量翻倍
 */
void xr_array_grow(XrArray *arr);

/**
 * 确保数组有足够容量
 * @param arr 数组
 * @param min_capacity 最小容量
 */
void xr_array_ensure_capacity(XrArray *arr, int min_capacity);

#endif /* XRAY_ARRAY_H */


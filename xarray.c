/* xarray.c - Xray 动态数组实现 */

#include "xarray.h"
#include "xmem.h"
#include "xvalue.h"
#include <stdio.h>
#include <string.h>

/* 前向声明（高阶方法需要） */
XrValue xr_eval_call_function(XrFunction *func, XrValue *args, int arg_count, struct XSymbolTable *symbols);
int xr_is_truthy(XrValue value);

/* 简单的值比较函数（用于数组的 indexOf） */
static bool xr_value_equal(XrValue a, XrValue b) {
    if (a.type != b.type) return false;
    
    switch (a.type) {
        case XR_TNULL:
            return true;
        case XR_TBOOL:
            return a.as.b == b.as.b;
        case XR_TINT:
            return a.as.i == b.as.i;
        case XR_TFLOAT:
            return a.as.n == b.as.n;
        case XR_TSTRING:
            // 简化：指针比较（实际应该比较字符串内容）
            return a.as.obj == b.as.obj;
        default:
            return a.as.obj == b.as.obj;
    }
}

/* ====== 创建和销毁 ====== */

XrArray* xr_array_new(void) {
    return xr_array_with_capacity(0);
}

XrArray* xr_array_with_capacity(int capacity) {
    // 分配数组对象（使用统一对象分配接口）
    XrArray *arr = XR_ALLOCATE(XrArray, OBJ_ARRAY);
    
    // 初始化字段
    arr->count = 0;
    arr->capacity = capacity;
    arr->element_type = NULL;  // 暂不设置类型
    
    // 分配元素数组
    if (capacity > 0) {
        arr->elements = xmem_alloc(sizeof(XrValue) * capacity);
    } else {
        arr->elements = NULL;
    }
    
    return arr;
}

XrArray* xr_array_from_values(XrValue *elements, int count) {
    XrArray *arr = xr_array_with_capacity(count);
    
    // 复制元素
    for (int i = 0; i < count; i++) {
        arr->elements[i] = elements[i];
    }
    arr->count = count;
    
    return arr;
}

void xr_array_free(XrArray *arr) {
    if (!arr) return;
    
    // 释放元素数组
    if (arr->elements) {
        xmem_free(arr->elements);
        arr->elements = NULL;
    }
    
    // 释放数组对象
    xmem_free(arr);
}

/* ====== 访问元素 ====== */

XrValue xr_array_get(XrArray *arr, int index) {
    // 边界检查
    if (index < 0 || index >= arr->count) {
        return xr_null();
    }
    
    return arr->elements[index];
}

void xr_array_set(XrArray *arr, int index, XrValue value) {
    // 边界检查
    if (index < 0 || index >= arr->count) {
        return;
    }
    
    arr->elements[index] = value;
}

int xr_array_length(XrArray *arr) {
    return arr->count;
}

/* ====== 修改数组 ====== */

void xr_array_push(XrArray *arr, XrValue value) {
    // 确保容量足够
    if (arr->count >= arr->capacity) {
        xr_array_grow(arr);
    }
    
    // 添加元素
    arr->elements[arr->count++] = value;
}

XrValue xr_array_pop(XrArray *arr) {
    // 空数组
    if (arr->count == 0) {
        return xr_null();
    }
    
    // 移除并返回最后一个元素
    return arr->elements[--arr->count];
}

void xr_array_unshift(XrArray *arr, XrValue value) {
    // 确保容量足够
    if (arr->count >= arr->capacity) {
        xr_array_grow(arr);
    }
    
    // 移动所有元素向后一位
    for (int i = arr->count; i > 0; i--) {
        arr->elements[i] = arr->elements[i - 1];
    }
    
    // 在开头插入新元素
    arr->elements[0] = value;
    arr->count++;
}

XrValue xr_array_shift(XrArray *arr) {
    // 空数组
    if (arr->count == 0) {
        return xr_null();
    }
    
    // 保存第一个元素
    XrValue first = arr->elements[0];
    
    // 移动所有元素向前一位
    for (int i = 0; i < arr->count - 1; i++) {
        arr->elements[i] = arr->elements[i + 1];
    }
    
    arr->count--;
    return first;
}

void xr_array_clear(XrArray *arr) {
    arr->count = 0;
}

/* ====== 查询方法 ====== */

int xr_array_index_of(XrArray *arr, XrValue value) {
    for (int i = 0; i < arr->count; i++) {
        // 使用值比较函数
        if (xr_value_equal(arr->elements[i], value)) {
            return i;
        }
    }
    return -1;  // 未找到
}

bool xr_array_contains(XrArray *arr, XrValue value) {
    return xr_array_index_of(arr, value) != -1;
}

bool xr_array_is_empty(XrArray *arr) {
    return arr->count == 0;
}

/* ====== 高阶方法 ====== */

void xr_array_foreach(XrArray *arr, XrFunction *callback, struct XSymbolTable *symbols) {
    /* 遍历每个元素，调用回调 */
    for (size_t i = 0; i < arr->count; i++) {
        XrValue args[2];
        args[0] = arr->elements[i];
        args[1] = xr_int(i);
        
        /* 调用回调函数 */
        xr_eval_call_function(callback, args, 2, symbols);
    }
}

XrArray* xr_array_map(XrArray *arr, XrFunction *callback, struct XSymbolTable *symbols) {
    /* 创建新数组 */
    XrArray *result = xr_array_with_capacity(arr->count);
    
    /* 映射每个元素 */
    for (size_t i = 0; i < arr->count; i++) {
        XrValue args[2];
        args[0] = arr->elements[i];
        args[1] = xr_int(i);
        
        /* 调用回调函数并获取返回值 */
        XrValue mapped = xr_eval_call_function(callback, args, 2, symbols);
        xr_array_push(result, mapped);
    }
    
    return result;
}

XrArray* xr_array_filter(XrArray *arr, XrFunction *callback, struct XSymbolTable *symbols) {
    /* 创建新数组 */
    XrArray *result = xr_array_with_capacity(arr->count / 2);
    
    /* 过滤每个元素 */
    for (size_t i = 0; i < arr->count; i++) {
        XrValue args[1];
        args[0] = arr->elements[i];
        
        /* 调用回调函数 */
        XrValue test_result = xr_eval_call_function(callback, args, 1, symbols);
        
        /* 如果返回 true，保留该元素 */
        if (xr_is_truthy(test_result)) {
            xr_array_push(result, arr->elements[i]);
        }
    }
    
    return result;
}

XrValue xr_array_reduce(XrArray *arr, XrFunction *callback, XrValue initial, struct XSymbolTable *symbols) {
    XrValue accumulator = initial;
    
    /* 归约每个元素 */
    for (size_t i = 0; i < arr->count; i++) {
        XrValue args[2];
        args[0] = accumulator;
        args[1] = arr->elements[i];
        
        /* 调用回调函数，更新累积值 */
        accumulator = xr_eval_call_function(callback, args, 2, symbols);
    }
    
    return accumulator;
}

/* ====== 工具方法 ====== */

void xr_array_reverse(XrArray *arr) {
    // 双指针交换
    int left = 0;
    int right = arr->count - 1;
    
    while (left < right) {
        // 交换元素
        XrValue temp = arr->elements[left];
        arr->elements[left] = arr->elements[right];
        arr->elements[right] = temp;
        
        left++;
        right--;
    }
}

XrArray* xr_array_copy(XrArray *arr) {
    return xr_array_from_values(arr->elements, arr->count);
}

void xr_array_print(XrArray *arr) {
    printf("[");
    for (size_t i = 0; i < arr->count; i++) {
        if (i > 0) printf(", ");
        // TODO: 使用 xr_value_print（Day 5 实现）
        // 暂时简单打印
        printf("?");
    }
    printf("]");
}

/* ====== 内部函数 ====== */

void xr_array_grow(XrArray *arr) {
    // 计算新容量
    int new_capacity = arr->capacity == 0 
        ? XR_ARRAY_INIT_CAPACITY 
        : arr->capacity * 2;
    
    // 重新分配内存
    arr->elements = xmem_realloc(
        arr->elements,
        sizeof(XrValue) * arr->capacity,
        sizeof(XrValue) * new_capacity
    );
    
    arr->capacity = new_capacity;
}

void xr_array_ensure_capacity(XrArray *arr, int min_capacity) {
    if (arr->capacity >= min_capacity) {
        return;
    }
    
    // 扩容到至少 min_capacity
    int new_capacity = arr->capacity;
    if (new_capacity == 0) {
        new_capacity = XR_ARRAY_INIT_CAPACITY;
    }
    
    while (new_capacity < min_capacity) {
        new_capacity *= 2;
    }
    
    // 重新分配内存
    arr->elements = xmem_realloc(
        arr->elements,
        sizeof(XrValue) * arr->capacity,
        sizeof(XrValue) * new_capacity
    );
    
    arr->capacity = new_capacity;
}


/*
 * 用分隔符连接数组元素为字符串（v0.10.0新增）
 */
struct XrString* xr_array_join(XrArray *arr, struct XrString *delimiter) {
    extern struct XrString* xr_string_intern(const char *chars, size_t length, uint32_t hash);
    extern struct XrString* xr_string_concat(struct XrString *a, struct XrString *b);
    extern struct XrString* xr_string_from_int(xr_Integer i);
    extern struct XrString* xr_string_from_float(xr_Number n);
    
    if (arr == NULL || arr->count == 0) {
        return xr_string_intern("", 0, 0);
    }
    
    struct XrString *result = xr_string_intern("", 0, 0);
    
    for (size_t i = 0; i < arr->count; i++) {
        XrValue val = arr->elements[i];
        struct XrString *str_part = NULL;
        
        if (xr_isstring(val)) {
            str_part = xr_tostring(val);
        } else if (xr_isint(val)) {
            str_part = xr_string_from_int(xr_toint(val));
        } else if (xr_isfloat(val)) {
            str_part = xr_string_from_float(xr_tofloat(val));
        } else if (xr_isbool(val)) {
            str_part = xr_tobool(val) ? 
                xr_string_intern("true", 4, 0) : 
                xr_string_intern("false", 5, 0);
        } else if (xr_isnull(val)) {
            str_part = xr_string_intern("null", 4, 0);
        } else {
            str_part = xr_string_intern("[object]", 8, 0);
        }
        
        if (str_part != NULL) {
            result = xr_string_concat(result, str_part);
        }
        
        if (i < arr->count - 1 && delimiter != NULL) {
            result = xr_string_concat(result, delimiter);
        }
    }
    
    return result;
}

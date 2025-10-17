/*
** xstring.c
** Xray 字符串对象实现
** 
** v0.10.0: 字符串增强
*/

#include "xvalue.h"   /* 值系统 */
#include "xtype.h"     /* 类型系统 */
#include "xmem.h"      /* 内存管理 */
#include "xarray.h"    /* 数组完整定义 - 必须在xstring.h之前 */
#include "xstring.h"   /* 字符串定义 */
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

/* 全局字符串池 */
static StringPool g_string_pool = {0};

/* ========== 字符串池管理 ========== */

/*
** 初始化字符串池
*/
void xr_string_pool_init(void) {
    g_string_pool.capacity = STRING_POOL_INIT_CAPACITY;
    g_string_pool.count = 0;
    g_string_pool.threshold = (size_t)(g_string_pool.capacity * STRING_POOL_LOAD_FACTOR);
    g_string_pool.entries = (XrString**)xmem_alloc(
        sizeof(XrString*) * g_string_pool.capacity
    );
    
    /* 初始化为NULL */
    for (size_t i = 0; i < g_string_pool.capacity; i++) {
        g_string_pool.entries[i] = NULL;
    }
}

/*
** 释放字符串池
*/
void xr_string_pool_free(void) {
    if (g_string_pool.entries == NULL) return;
    
    /* 释放所有字符串 */
    for (size_t i = 0; i < g_string_pool.capacity; i++) {
        if (g_string_pool.entries[i] != NULL) {
            xr_string_free(g_string_pool.entries[i]);
        }
    }
    
    /* 释放表 */
    xmem_free(g_string_pool.entries);
    g_string_pool.entries = NULL;
    g_string_pool.capacity = 0;
    g_string_pool.count = 0;
}

/*
** 扩容字符串池
*/
void xr_string_pool_grow(void) {
    size_t old_capacity = g_string_pool.capacity;
    XrString **old_entries = g_string_pool.entries;
    
    /* 容量翻倍 */
    g_string_pool.capacity = old_capacity * 2;
    g_string_pool.threshold = (size_t)(g_string_pool.capacity * STRING_POOL_LOAD_FACTOR);
    g_string_pool.entries = (XrString**)xmem_alloc(
        sizeof(XrString*) * g_string_pool.capacity
    );
    
    /* 初始化为NULL */
    for (size_t i = 0; i < g_string_pool.capacity; i++) {
        g_string_pool.entries[i] = NULL;
    }
    
    /* 重新哈希所有字符串 */
    g_string_pool.count = 0;  /* 重新计数 */
    for (size_t i = 0; i < old_capacity; i++) {
        XrString *str = old_entries[i];
        if (str != NULL) {
            /* 重新插入 */
            uint32_t index = str->hash % g_string_pool.capacity;
            
            /* 线性探测 */
            while (g_string_pool.entries[index] != NULL) {
                index = (index + 1) % g_string_pool.capacity;
            }
            
            g_string_pool.entries[index] = str;
            g_string_pool.count++;
        }
    }
    
    /* 释放旧表 */
    xmem_free(old_entries);
}

/*
** 获取字符串池统计信息
*/
void xr_string_pool_stats(size_t *count, size_t *capacity, double *load_factor) {
    if (count) *count = g_string_pool.count;
    if (capacity) *capacity = g_string_pool.capacity;
    if (load_factor) {
        *load_factor = g_string_pool.capacity > 0 
            ? (double)g_string_pool.count / g_string_pool.capacity 
            : 0.0;
    }
}

/* ========== 字符串哈希 ========== */

/*
** FNV-1a 哈希算法
** 
** 算法步骤：
** 1. hash = 2166136261 (FNV offset basis)
** 2. 对每个字节：
**    hash = (hash ^ byte) * 16777619 (FNV prime)
*/
uint32_t xr_string_hash(const char *chars, size_t length) {
    uint32_t hash = 2166136261u;  /* FNV offset basis */
    
    for (size_t i = 0; i < length; i++) {
        hash ^= (uint8_t)chars[i];
        hash *= 16777619u;  /* FNV prime */
    }
    
    return hash;
}

/* ========== 字符串创建 ========== */

/*
** 创建新字符串（不驻留）
*/
XrString* xr_string_new(const char *chars, size_t length) {
    /* 分配字符串对象 */
    XrString *str = (XrString*)xmem_alloc(sizeof(XrString));
    
    /* 初始化对象头 */
    str->header.type = XR_TSTRING;
    str->header.type_info = NULL;  /* 字符串类型信息由类型系统管理 */
    str->header.next = NULL;
    str->header.marked = false;
    
    /* 初始化字符串数据 */
    str->length = length;
    str->hash = xr_string_hash(chars, length);
    
    /* 分配并复制字符数据 */
    str->chars = (char*)xmem_alloc(length + 1);
    memcpy(str->chars, chars, length);
    str->chars[length] = '\0';
    
    return str;
}

/*
** 从C字符串创建
*/
XrString* xr_string_copy(const char *chars) {
    return xr_string_new(chars, strlen(chars));
}

/*
** 字符串驻留
*/
XrString* xr_string_intern(const char *chars, size_t length, uint32_t hash) {
    /* 如果池未初始化，先初始化 */
    if (g_string_pool.entries == NULL) {
        xr_string_pool_init();
    }
    
    /* 如果哈希值为0，计算哈希 */
    if (hash == 0) {
        hash = xr_string_hash(chars, length);
    }
    
    /* 在池中查找 */
    uint32_t index = hash % g_string_pool.capacity;
    
    for (;;) {
        XrString *entry = g_string_pool.entries[index];
        
        if (entry == NULL) {
            /* 未找到，创建新字符串 */
            XrString *str = xr_string_new(chars, length);
            str->hash = hash;
            
            /* 插入池中 */
            g_string_pool.entries[index] = str;
            g_string_pool.count++;
            
            /* 检查是否需要扩容 */
            if (g_string_pool.count > g_string_pool.threshold) {
                xr_string_pool_grow();
            }
            
            return str;
        }
        
        /* 检查是否匹配 */
        if (entry->length == length && 
            entry->hash == hash &&
            memcmp(entry->chars, chars, length) == 0) {
            /* 找到相同字符串 */
            return entry;
        }
        
        /* 冲突，线性探测 */
        index = (index + 1) % g_string_pool.capacity;
    }
}

/*
** 拼接两个字符串
*/
XrString* xr_string_concat(XrString *a, XrString *b) {
    if (a == NULL || b == NULL) return NULL;
    
    size_t new_length = a->length + b->length;
    char *buffer = (char*)xmem_alloc(new_length + 1);
    
    memcpy(buffer, a->chars, a->length);
    memcpy(buffer + a->length, b->chars, b->length);
    buffer[new_length] = '\0';
    
    /* 驻留结果 */
    XrString *result = xr_string_intern(buffer, new_length, 0);
    xmem_free(buffer);
    
    return result;
}

/*
** 从整数创建字符串
*/
XrString* xr_string_from_int(xr_Integer i) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%lld", (long long)i);
    return xr_string_intern(buffer, strlen(buffer), 0);
}

/*
** 从浮点数创建字符串
*/
XrString* xr_string_from_float(xr_Number n) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.15g", n);
    return xr_string_intern(buffer, strlen(buffer), 0);
}

/* ========== 字符串比较 ========== */

/*
** 字符串字典序比较
*/
int xr_string_compare(XrString *a, XrString *b) {
    if (a == NULL || b == NULL) {
        if (a == b) return 0;
        return a == NULL ? -1 : 1;
    }
    
    /* 先比较指针 */
    if (a == b) return 0;
    
    /* 字典序比较 */
    size_t min_len = a->length < b->length ? a->length : b->length;
    int cmp = memcmp(a->chars, b->chars, min_len);
    
    if (cmp != 0) return cmp;
    
    /* 长度不同 */
    if (a->length < b->length) return -1;
    if (a->length > b->length) return 1;
    return 0;
}

/* ========== 字符串基础方法 ========== */

/*
** charAt - 获取指定位置的字符
*/
XrString* xr_string_char_at(XrString *str, xr_Integer index) {
    if (str == NULL) return NULL;
    
    /* 边界检查 */
    if (index < 0 || (size_t)index >= str->length) {
        return NULL;
    }
    
    /* 创建单字符字符串 */
    return xr_string_intern(&str->chars[index], 1, 0);
}

/*
** substring - 截取子串
*/
XrString* xr_string_substring(XrString *str, xr_Integer start, xr_Integer end) {
    if (str == NULL) return NULL;
    
    /* 处理负数索引 */
    if (start < 0) start = 0;
    if (end < 0 || (size_t)end > str->length) end = str->length;
    
    /* 边界检查 */
    if (start >= end || (size_t)start >= str->length) {
        return xr_string_intern("", 0, 0);
    }
    
    /* 截取子串 */
    size_t len = end - start;
    return xr_string_intern(&str->chars[start], len, 0);
}

/*
** indexOf - 查找子串位置
*/
xr_Integer xr_string_index_of(XrString *str, XrString *substr) {
    if (str == NULL || substr == NULL) return -1;
    if (substr->length == 0) return 0;
    if (substr->length > str->length) return -1;
    
    /* 暴力搜索（后续可以优化为KMP或Boyer-Moore）*/
    for (size_t i = 0; i <= str->length - substr->length; i++) {
        if (memcmp(&str->chars[i], substr->chars, substr->length) == 0) {
            return (xr_Integer)i;
        }
    }
    
    return -1;
}

/*
** contains - 检查是否包含子串
*/
bool xr_string_contains(XrString *str, XrString *substr) {
    return xr_string_index_of(str, substr) >= 0;
}

/*
** startsWith - 检查前缀
*/
bool xr_string_starts_with(XrString *str, XrString *prefix) {
    if (str == NULL || prefix == NULL) return false;
    if (prefix->length > str->length) return false;
    if (prefix->length == 0) return true;
    
    return memcmp(str->chars, prefix->chars, prefix->length) == 0;
}

/*
** endsWith - 检查后缀
*/
bool xr_string_ends_with(XrString *str, XrString *suffix) {
    if (str == NULL || suffix == NULL) return false;
    if (suffix->length > str->length) return false;
    if (suffix->length == 0) return true;
    
    size_t offset = str->length - suffix->length;
    return memcmp(&str->chars[offset], suffix->chars, suffix->length) == 0;
}

/*
** toLowerCase - 转小写
*/
XrString* xr_string_to_lower_case(XrString *str) {
    if (str == NULL) return NULL;
    
    char *buffer = (char*)xmem_alloc(str->length + 1);
    
    for (size_t i = 0; i < str->length; i++) {
        buffer[i] = tolower((unsigned char)str->chars[i]);
    }
    buffer[str->length] = '\0';
    
    XrString *result = xr_string_intern(buffer, str->length, 0);
    xmem_free(buffer);
    
    return result;
}

/*
** toUpperCase - 转大写
*/
XrString* xr_string_to_upper_case(XrString *str) {
    if (str == NULL) return NULL;
    
    char *buffer = (char*)xmem_alloc(str->length + 1);
    
    for (size_t i = 0; i < str->length; i++) {
        buffer[i] = toupper((unsigned char)str->chars[i]);
    }
    buffer[str->length] = '\0';
    
    XrString *result = xr_string_intern(buffer, str->length, 0);
    xmem_free(buffer);
    
    return result;
}

/*
** trim - 去除首尾空白
*/
XrString* xr_string_trim(XrString *str) {
    if (str == NULL) return NULL;
    if (str->length == 0) return str;
    
    /* 找到第一个非空白字符 */
    size_t start = 0;
    while (start < str->length && xr_is_whitespace(str->chars[start])) {
        start++;
    }
    
    /* 全是空白 */
    if (start == str->length) {
        return xr_string_intern("", 0, 0);
    }
    
    /* 找到最后一个非空白字符 */
    size_t end = str->length - 1;
    while (end > start && xr_is_whitespace(str->chars[end])) {
        end--;
    }
    
    /* 截取 */
    size_t len = end - start + 1;
    return xr_string_intern(&str->chars[start], len, 0);
}

/* ========== 字符串高级方法 ========== */

/*
** split - 分割字符串为数组
*/
struct XrArray* xr_string_split(XrString *str, XrString *delimiter) {
    struct XrArray *result = xr_array_new();
    
    if (str == NULL) return result;
    
    /* 空分隔符，按字符分割 */
    if (delimiter == NULL || delimiter->length == 0) {
        for (size_t i = 0; i < str->length; i++) {
            XrString *ch = xr_string_intern(&str->chars[i], 1, 0);
            xr_array_push(result, xr_string_value(ch));
        }
        return result;
    }
    
    /* 按分隔符分割 */
    const char *start = str->chars;
    const char *end = str->chars;
    const char *str_end = str->chars + str->length;
    
    while (end <= str_end - delimiter->length) {
        if (memcmp(end, delimiter->chars, delimiter->length) == 0) {
            /* 找到分隔符 */
            size_t len = end - start;
            XrString *part = xr_string_intern(start, len, 0);
            xr_array_push(result, xr_string_value(part));
            
            end += delimiter->length;
            start = end;
        } else {
            end++;
        }
    }
    
    /* 添加最后一部分 */
    size_t len = str_end - start;
    XrString *part = xr_string_intern(start, len, 0);
    xr_array_push(result, xr_string_value(part));
    
    return result;
}

/*
** replace - 替换子串（首次）
*/
XrString* xr_string_replace(XrString *str, XrString *old_str, XrString *new_str) {
    if (str == NULL || old_str == NULL || new_str == NULL) return str;
    if (old_str->length == 0) return str;
    
    /* 查找第一次出现的位置 */
    xr_Integer pos = xr_string_index_of(str, old_str);
    if (pos < 0) return str;  /* 未找到 */
    
    /* 计算新长度 */
    size_t new_length = str->length - old_str->length + new_str->length;
    char *buffer = (char*)xmem_alloc(new_length + 1);
    
    /* 拼接：前缀 + new_str + 后缀 */
    memcpy(buffer, str->chars, pos);
    memcpy(buffer + pos, new_str->chars, new_str->length);
    memcpy(buffer + pos + new_str->length, 
           str->chars + pos + old_str->length,
           str->length - pos - old_str->length);
    buffer[new_length] = '\0';
    
    XrString *result = xr_string_intern(buffer, new_length, 0);
    xmem_free(buffer);
    
    return result;
}

/*
** replaceAll - 替换所有匹配
** 注意：join 已经移到xarray.c，这里需要用循环实现
*/
XrString* xr_string_replace_all(XrString *str, XrString *old_str, XrString *new_str) {
    if (str == NULL || old_str == NULL || new_str == NULL) return str;
    if (old_str->length == 0) return str;
    
    /* 简单实现：重复调用replace直到没有匹配 */
    XrString *result = str;
    xr_Integer pos;
    
    while ((pos = xr_string_index_of(result, old_str)) >= 0) {
        result = xr_string_replace(result, old_str, new_str);
    }
    
    return result;
}

/*
** repeat - 重复字符串
*/
XrString* xr_string_repeat(XrString *str, xr_Integer count) {
    if (str == NULL || count <= 0) {
        return xr_string_intern("", 0, 0);
    }
    if (count == 1) return str;
    
    size_t new_length = str->length * count;
    char *buffer = (char*)xmem_alloc(new_length + 1);
    
    for (xr_Integer i = 0; i < count; i++) {
        memcpy(buffer + i * str->length, str->chars, str->length);
    }
    buffer[new_length] = '\0';
    
    XrString *result = xr_string_intern(buffer, new_length, 0);
    xmem_free(buffer);
    
    return result;
}

/*
** 注意：join 方法已移至 xarray.c
** 因为 join 是数组的方法: array.join(delimiter)
** 而不是字符串的方法
*/

/* ========== 字符串释放 ========== */

/*
** 释放字符串对象
*/
void xr_string_free(XrString *str) {
    if (str == NULL) return;
    
    if (str->chars != NULL) {
        xmem_free(str->chars);
    }
    
    xmem_free(str);
}

/* ========== 辅助函数 ========== */

/*
** 打印字符串（调试用）
*/
void xr_string_print(XrString *str) {
    if (str == NULL) {
        printf("(null)");
    } else {
        printf("%.*s", (int)str->length, str->chars);
    }
}


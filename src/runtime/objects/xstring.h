/*
** xstring.h
** Xray 字符串对象系统
** 
** v0.10.0: 字符串增强
** 设计特点：
**   - 不可变字符串（immutable）
**   - 自动驻留（string interning）
**   - 哈希值缓存
**   - UTF-8字节级操作
** 
** 参考：
**   - Lua的字符串驻留策略
**   - Wren的ObjString设计
*/

#ifndef xstring_h
#define xstring_h

#include "xray.h"
#include "xvalue.h"  /* 直接包含，获取所有类型定义 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* 前向声明 */
struct XrArray;

/* ========== 字符串对象 ========== */

/*
** 字符串对象
** 
** 设计要点：
** 1. 不可变：创建后不能修改内容
** 2. 驻留：相同内容的字符串只存储一份
** 3. 哈希缓存：创建时计算哈希值，避免重复计算
** 4. 引用计数：通过GC管理（未来）
*/
typedef struct XrString {
    XrObject header;        /* GC对象头（包含type, type_info, next, marked）*/
    size_t length;          /* 字符串长度（字节数，UTF-8）*/
    uint32_t hash;          /* 哈希值（FNV-1a算法）*/
    char *chars;            /* 字符串数据（以\0结尾）*/
} XrString;

/* ========== 字符串驻留池 ========== */

/*
** 字符串驻留池
** 
** 采用开放寻址哈希表（线性探测）
** 参考Lua的字符串表设计
*/
typedef struct {
    XrString **entries;     /* 字符串数组 */
    size_t capacity;        /* 容量 */
    size_t count;           /* 字符串数量 */
    size_t threshold;       /* 扩容阈值（capacity * 0.75）*/
} StringPool;

/* 字符串池初始容量 */
#define STRING_POOL_INIT_CAPACITY 128
#define STRING_POOL_LOAD_FACTOR   0.75

/* ========== 字符串创建 ========== */

/*
** 创建新字符串（不驻留）
** 参数：
**   chars: 字符数据
**   length: 字符串长度
** 返回：
**   新创建的字符串对象
*/
XrString* xr_string_new(const char *chars, size_t length);

/*
** 从C字符串创建（不驻留）
** 参数：
**   chars: C字符串（\0结尾）
** 返回：
**   新创建的字符串对象
*/
XrString* xr_string_copy(const char *chars);

/*
** 拼接两个字符串（结果驻留）
** 参数：
**   a, b: 要拼接的字符串
** 返回：
**   拼接后的新字符串（驻留）
*/
XrString* xr_string_concat(XrString *a, XrString *b);

/*
** 从整数创建字符串（驻留）
** 参数：
**   i: 整数值
** 返回：
**   字符串表示（如 "123"）
*/
XrString* xr_string_from_int(xr_Integer i);

/*
** 从浮点数创建字符串（驻留）
** 参数：
**   n: 浮点数值
** 返回：
**   字符串表示（如 "3.14"）
*/
XrString* xr_string_from_float(xr_Number n);

/* ========== 字符串驻留 ========== */

/*
** 字符串驻留
** 
** 如果池中已存在相同内容的字符串，返回已有字符串
** 否则将新字符串加入池中
** 
** 参数：
**   chars: 字符数据
**   length: 字符串长度
**   hash: 哈希值（如果为0则自动计算）
** 返回：
**   驻留的字符串对象
*/
XrString* xr_string_intern(const char *chars, size_t length, uint32_t hash);

/* ========== 字符串池管理 ========== */

/*
** 初始化字符串池
** 在程序启动时调用
*/
void xr_string_pool_init(void);

/*
** 释放字符串池
** 在程序退出时调用
*/
void xr_string_pool_free(void);

/*
** 扩容字符串池
** 当负载因子超过阈值时自动调用
*/
void xr_string_pool_grow(void);

/*
** 获取字符串池统计信息
*/
void xr_string_pool_stats(size_t *count, size_t *capacity, double *load_factor);

/* ========== 字符串比较 ========== */

/*
** 字符串相等比较
** 由于驻留，可以直接比较指针
** 
** 参数：
**   a, b: 要比较的字符串
** 返回：
**   相等返回true，否则返回false
*/
static inline bool xr_string_equal(XrString *a, XrString *b) {
    /* 驻留字符串可以直接比较指针 */
    if (a == b) return true;
    if (a == NULL || b == NULL) return false;
    if (a->length != b->length) return false;
    if (a->hash != b->hash) return false;
    return memcmp(a->chars, b->chars, a->length) == 0;
}

/*
** 字符串字典序比较
** 
** 参数：
**   a, b: 要比较的字符串
** 返回：
**   a < b: 返回负数
**   a == b: 返回0
**   a > b: 返回正数
*/
int xr_string_compare(XrString *a, XrString *b);

/* ========== 字符串哈希 ========== */

/*
** 计算字符串哈希值（FNV-1a算法）
** 
** FNV-1a特点：
**   - 简单高效（只需异或和乘法）
**   - 良好的分布特性
**   - Wren、Lua等语言都使用类似算法
** 
** 参数：
**   chars: 字符数据
**   length: 字符串长度
** 返回：
**   32位哈希值
*/
uint32_t xr_string_hash(const char *chars, size_t length);

/* ========== 字符串方法 ========== */

/*
** charAt - 获取指定位置的字符
** 参数：
**   str: 字符串
**   index: 字符索引
** 返回：
**   单字符字符串，越界返回NULL
*/
XrString* xr_string_char_at(XrString *str, xr_Integer index);

/*
** substring - 截取子串
** 参数：
**   str: 字符串
**   start: 起始位置
**   end: 结束位置（-1表示到末尾）
** 返回：
**   子串（驻留）
*/
XrString* xr_string_substring(XrString *str, xr_Integer start, xr_Integer end);

/*
** indexOf - 查找子串位置
** 参数：
**   str: 字符串
**   substr: 要查找的子串
** 返回：
**   子串首次出现的位置，未找到返回-1
*/
xr_Integer xr_string_index_of(XrString *str, XrString *substr);

/*
** contains - 检查是否包含子串
** 参数：
**   str: 字符串
**   substr: 要查找的子串
** 返回：
**   包含返回true，否则返回false
*/
bool xr_string_contains(XrString *str, XrString *substr);

/*
** startsWith - 检查前缀
** 参数：
**   str: 字符串
**   prefix: 前缀
** 返回：
**   以prefix开头返回true，否则返回false
*/
bool xr_string_starts_with(XrString *str, XrString *prefix);

/*
** endsWith - 检查后缀
** 参数：
**   str: 字符串
**   suffix: 后缀
** 返回：
**   以suffix结尾返回true，否则返回false
*/
bool xr_string_ends_with(XrString *str, XrString *suffix);

/*
** toLowerCase - 转小写
** 参数：
**   str: 字符串
** 返回：
**   小写字符串（驻留）
*/
XrString* xr_string_to_lower_case(XrString *str);

/*
** toUpperCase - 转大写
** 参数：
**   str: 字符串
** 返回：
**   大写字符串（驻留）
*/
XrString* xr_string_to_upper_case(XrString *str);

/*
** trim - 去除首尾空白字符
** 参数：
**   str: 字符串
** 返回：
**   去除空白后的字符串（驻留）
*/
XrString* xr_string_trim(XrString *str);

/* ========== 字符串高级方法 ========== */

/*
** split - 分割字符串为数组
** 参数：
**   str: 字符串
**   delimiter: 分隔符
** 返回：
**   字符串数组
*/
struct XrArray* xr_string_split(XrString *str, XrString *delimiter);

/*
** replace - 替换子串（首次）
** 参数：
**   str: 字符串
**   old_str: 要替换的子串
**   new_str: 新子串
** 返回：
**   替换后的字符串（驻留）
*/
XrString* xr_string_replace(XrString *str, XrString *old_str, XrString *new_str);

/*
** replaceAll - 替换所有匹配的子串
** 参数：
**   str: 字符串
**   old_str: 要替换的子串
**   new_str: 新子串
** 返回：
**   替换后的字符串（驻留）
*/
XrString* xr_string_replace_all(XrString *str, XrString *old_str, XrString *new_str);

/*
** repeat - 重复字符串
** 参数：
**   str: 字符串
**   count: 重复次数
** 返回：
**   重复后的字符串（驻留）
*/
XrString* xr_string_repeat(XrString *str, xr_Integer count);

/*
** 注意：join 方法已移至 xarray.h/c
** 因为 join 是数组的方法，不是字符串的方法
** 用法: array.join(",") 而不是 ",".join(array)
*/

/* ========== 字符串释放 ========== */

/*
** 释放字符串对象
** 注意：驻留的字符串由字符串池管理，不应直接释放
** 
** 参数：
**   str: 要释放的字符串
*/
void xr_string_free(XrString *str);

/* ========== 辅助函数 ========== */

/*
** 检查字符是否为空白字符
*/
static inline bool xr_is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/*
** 打印字符串（调试用）
*/
void xr_string_print(XrString *str);

#endif /* xstring_h */


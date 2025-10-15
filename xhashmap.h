/*
** xhashmap.h
** Xray 字符串哈希表（用于类系统）
** 
** v0.12.0：第十二阶段 - OOP系统
** 
** 简化设计：
** 1. 键为字符串（方法名、字段名等）
** 2. 值为void*指针（指向XrMethod等对象）
** 3. 开放寻址+线性探测
*/

#ifndef xhashmap_h
#define xhashmap_h

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/*
** 哈希表条目
*/
typedef struct {
    char *key;          /* 键（字符串，NULL表示空槽位）*/
    void *value;        /* 值（void*指针）*/
    bool is_tombstone;  /* 墓碑标记 */
} XrHashMapEntry;

/*
** 哈希表对象
*/
typedef struct {
    XrHashMapEntry *entries;  /* 条目数组 */
    uint32_t capacity;        /* 容量（2的幂）*/
    uint32_t count;           /* 实际元素数量 */
} XrHashMap;

/* ========== 哈希表操作 ========== */

/*
** 创建新哈希表
** 
** @return 新创建的哈希表
*/
XrHashMap* xr_hashmap_new(void);

/*
** 释放哈希表
** 
** @param map 哈希表对象
*/
void xr_hashmap_free(XrHashMap *map);

/*
** 设置键值对
** 
** @param map 哈希表对象
** @param key 键（字符串）
** @param value 值（void*指针）
*/
void xr_hashmap_set(XrHashMap *map, const char *key, void *value);

/*
** 获取键对应的值
** 
** @param map 哈希表对象
** @param key 键（字符串）
** @return 值（NULL表示未找到）
*/
void* xr_hashmap_get(XrHashMap *map, const char *key);

/*
** 检查键是否存在
** 
** @param map 哈希表对象
** @param key 键（字符串）
** @return true表示存在
*/
bool xr_hashmap_has(XrHashMap *map, const char *key);

/*
** 删除键值对
** 
** @param map 哈希表对象
** @param key 键（字符串）
** @return true表示成功删除
*/
bool xr_hashmap_delete(XrHashMap *map, const char *key);

/*
** 清空哈希表
** 
** @param map 哈希表对象
*/
void xr_hashmap_clear(XrHashMap *map);

/* ========== 常量定义 ========== */

#define XR_HASHMAP_MIN_CAPACITY 8
#define XR_HASHMAP_LOAD_FACTOR 0.75
#define XR_HASHMAP_GROW_FACTOR 2

#endif /* xhashmap_h */


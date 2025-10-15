/*
** xhashmap.c
** Xray 字符串哈希表实现
** 
** v0.12.0：第十二阶段 - OOP系统
*/

#include "xhashmap.h"
#include "xmem.h"
#include <string.h>
#include <assert.h>

/* ========== 内部辅助函数 ========== */

/*
** 字符串哈希函数（FNV-1a算法）
*/
static uint32_t hash_string(const char *str) {
    uint32_t hash = 2166136261u;  /* FNV offset basis */
    while (*str) {
        hash ^= (uint8_t)*str;
        hash *= 16777619u;  /* FNV prime */
        str++;
    }
    return hash;
}

/*
** 查找条目
** 
** @return 条目指针（NULL表示未找到）
** @param out_index 输出找到的索引或可插入的索引
*/
static XrHashMapEntry* find_entry(XrHashMap *map, const char *key, 
                                   uint32_t *out_index) {
    if (!map || !key) return NULL;
    
    uint32_t hash = hash_string(key);
    uint32_t index = hash & (map->capacity - 1);  /* 位运算取模 */
    
    XrHashMapEntry *tombstone = NULL;
    
    /* 线性探测 */
    for (uint32_t i = 0; i < map->capacity; i++) {
        uint32_t probe_index = (index + i) & (map->capacity - 1);
        XrHashMapEntry *entry = &map->entries[probe_index];
        
        if (entry->key == NULL) {
            if (!entry->is_tombstone) {
                /* 空槽位 */
                if (out_index) {
                    *out_index = tombstone ? (tombstone - map->entries) : probe_index;
                }
                return NULL;
            } else {
                /* 墓碑，记录但继续查找 */
                if (!tombstone) {
                    tombstone = entry;
                }
            }
        } else if (strcmp(entry->key, key) == 0) {
            /* 找到 */
            if (out_index) *out_index = probe_index;
            return entry;
        }
    }
    
    /* 表满或未找到 */
    if (out_index && tombstone) {
        *out_index = tombstone - map->entries;
    }
    return NULL;
}

/*
** 扩容哈希表
*/
static void resize(XrHashMap *map, uint32_t new_capacity) {
    assert(new_capacity >= map->capacity);
    
    /* 分配新条目数组 */
    XrHashMapEntry *new_entries = (XrHashMapEntry*)xmem_alloc(
        sizeof(XrHashMapEntry) * new_capacity);
    
    /* 初始化为空 */
    for (uint32_t i = 0; i < new_capacity; i++) {
        new_entries[i].key = NULL;
        new_entries[i].value = NULL;
        new_entries[i].is_tombstone = false;
    }
    
    /* 重新插入所有条目 */
    XrHashMapEntry *old_entries = map->entries;
    uint32_t old_capacity = map->capacity;
    
    map->entries = new_entries;
    map->capacity = new_capacity;
    map->count = 0;  /* 重新计数 */
    
    for (uint32_t i = 0; i < old_capacity; i++) {
        XrHashMapEntry *entry = &old_entries[i];
        if (entry->key != NULL && !entry->is_tombstone) {
            xr_hashmap_set(map, entry->key, entry->value);
        }
    }
    
    /* 释放旧数组 */
    xmem_free(old_entries);
}

/* ========== 哈希表操作实现 ========== */

/*
** 创建新哈希表
*/
XrHashMap* xr_hashmap_new(void) {
    XrHashMap *map = (XrHashMap*)xmem_alloc(sizeof(XrHashMap));
    
    map->capacity = XR_HASHMAP_MIN_CAPACITY;
    map->count = 0;
    map->entries = (XrHashMapEntry*)xmem_alloc(
        sizeof(XrHashMapEntry) * map->capacity);
    
    /* 初始化为空 */
    for (uint32_t i = 0; i < map->capacity; i++) {
        map->entries[i].key = NULL;
        map->entries[i].value = NULL;
        map->entries[i].is_tombstone = false;
    }
    
    return map;
}

/*
** 释放哈希表
*/
void xr_hashmap_free(XrHashMap *map) {
    if (!map) return;
    
    /* 释放所有键的字符串（假设键是动态分配的）*/
    /* 注意：值不在这里释放，由调用者管理 */
    
    if (map->entries) {
        xmem_free(map->entries);
    }
    
    xmem_free(map);
}

/*
** 设置键值对
*/
void xr_hashmap_set(XrHashMap *map, const char *key, void *value) {
    assert(map != NULL);
    assert(key != NULL);
    
    /* 检查是否需要扩容 */
    if ((double)map->count >= (double)map->capacity * XR_HASHMAP_LOAD_FACTOR) {
        resize(map, map->capacity * XR_HASHMAP_GROW_FACTOR);
    }
    
    /* 查找或插入 */
    uint32_t index;
    XrHashMapEntry *entry = find_entry(map, key, &index);
    
    if (!entry) {
        /* 新插入 */
        entry = &map->entries[index];
        entry->key = (char*)key;  /* 注意：不复制字符串，调用者负责 */
        entry->is_tombstone = false;
        map->count++;
    }
    
    entry->value = value;
}

/*
** 获取键对应的值
*/
void* xr_hashmap_get(XrHashMap *map, const char *key) {
    if (!map || !key) return NULL;
    
    uint32_t index;
    XrHashMapEntry *entry = find_entry(map, key, &index);
    
    return entry ? entry->value : NULL;
}

/*
** 检查键是否存在
*/
bool xr_hashmap_has(XrHashMap *map, const char *key) {
    if (!map || !key) return false;
    
    uint32_t index;
    XrHashMapEntry *entry = find_entry(map, key, &index);
    
    return entry != NULL;
}

/*
** 删除键值对
*/
bool xr_hashmap_delete(XrHashMap *map, const char *key) {
    if (!map || !key) return false;
    
    uint32_t index;
    XrHashMapEntry *entry = find_entry(map, key, &index);
    
    if (!entry) return false;
    
    /* 标记为墓碑 */
    entry->key = NULL;
    entry->value = NULL;
    entry->is_tombstone = true;
    map->count--;
    
    return true;
}

/*
** 清空哈希表
*/
void xr_hashmap_clear(XrHashMap *map) {
    if (!map) return;
    
    /* 重置所有条目 */
    for (uint32_t i = 0; i < map->capacity; i++) {
        map->entries[i].key = NULL;
        map->entries[i].value = NULL;
        map->entries[i].is_tombstone = false;
    }
    
    map->count = 0;
}


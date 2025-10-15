/* xmap.c - Map字典实现
 * 
 * 第11阶段 - Map字典实现
 */

#include "xmap.h"
#include "xhash.h"
#include "xobject.h"
#include "xmem.h"
#include "xarray.h"  /* 用于keys/values/entries */
#include <string.h>
#include <stdio.h>

/* ========== 辅助函数 ========== */

/**
 * 检查条目是否为空
 */
static inline bool entry_is_empty(XrMapEntry *entry) {
    return entry->state == XR_MAP_EMPTY;
}

/**
 * 检查条目是否为墓碑
 */
static inline bool entry_is_tombstone(XrMapEntry *entry) {
    return entry->state == XR_MAP_TOMBSTONE;
}

/**
 * 检查条目是否有效
 */
static inline bool entry_is_valid(XrMapEntry *entry) {
    return entry->state >= XR_MAP_VALID;
}

/**
 * 计算下一个容量（2的幂）
 */
static uint32_t next_capacity(uint32_t current) {
    if (current < XR_MAP_MIN_CAPACITY) {
        return XR_MAP_MIN_CAPACITY;
    }
    
    /* 防止溢出 */
    if (current > (UINT32_MAX / XR_MAP_GROW_FACTOR)) {
        return UINT32_MAX;
    }
    
    return current * XR_MAP_GROW_FACTOR;
}

/* ========== 创建和销毁 ========== */

XrMap* xr_map_new(void) {
    XrMap *map = XR_ALLOCATE(XrMap, OBJ_MAP);
    
    map->capacity = 0;
    map->count = 0;
    map->entries = NULL;
    map->flags = 0;
    map->gc_data = NULL;
    
    return map;
}

void xr_map_free(XrMap *map) {
    if (map->entries != NULL) {
        xmem_free(map->entries);
    }
    xr_free_object(map);
}

/* ========== 小Map线性扫描 ========== */

XrMapEntry* xr_map_find_linear(XrMap *map, XrValue key, uint32_t *out_index) {
    /* 空Map */
    if (map->capacity == 0) {
        *out_index = 0;
        return NULL;
    }
    
    uint32_t first_empty = UINT32_MAX;
    uint32_t first_tombstone = UINT32_MAX;
    
    /* 线性扫描所有槽位 */
    for (uint32_t i = 0; i < map->capacity; i++) {
        XrMapEntry *entry = &map->entries[i];
        
        if (entry_is_empty(entry)) {
            /* 记录第一个空槽位 */
            if (first_empty == UINT32_MAX) {
                first_empty = i;
            }
            /* 遇到空槽位停止查找（未找到） */
            *out_index = (first_tombstone != UINT32_MAX) ? first_tombstone : first_empty;
            return NULL;
        } else if (entry_is_tombstone(entry)) {
            /* 记录第一个墓碑 */
            if (first_tombstone == UINT32_MAX) {
                first_tombstone = i;
            }
        } else if (xr_map_keys_equal(entry->key, key)) {
            /* 找到匹配的键 */
            *out_index = i;
            return entry;
        }
    }
    
    /* 未找到，返回可插入位置 */
    *out_index = (first_tombstone != UINT32_MAX) ? first_tombstone : first_empty;
    return NULL;
}

/* ========== 核心查找算法 ========== */

XrMapEntry* xr_map_find_entry(XrMap *map, XrValue key, uint32_t *out_index) {
    /* 空Map */
    if (map->capacity == 0) {
        *out_index = 0;
        return NULL;
    }
    
    /* 小Map优化：使用线性扫描 */
    if (map->count <= XR_MAP_SMALL_SIZE) {
        return xr_map_find_linear(map, key, out_index);
    }
    
    /* 计算哈希和短哈希 */
    uint32_t hash = xr_hash_value(key);
    uint8_t sh = xr_short_hash(hash);
    
    /* 使用位运算代替模运算（capacity是2的幂） */
    uint32_t index = hash & (map->capacity - 1);
    uint32_t tombstone_index = UINT32_MAX;
    
    /* 线性探测 */
    for (;;) {
        XrMapEntry *entry = &map->entries[index];
        
        if (entry_is_empty(entry)) {
            /* 空槽位：未找到 */
            *out_index = (tombstone_index != UINT32_MAX) ? tombstone_index : index;
            return NULL;
        } else if (entry_is_tombstone(entry)) {
            /* 墓碑：记录但继续查找 */
            if (tombstone_index == UINT32_MAX) {
                tombstone_index = index;
            }
        } else if (entry->state == sh) {
            /* 短哈希匹配：进一步比较键 */
            if (xr_map_keys_equal(entry->key, key)) {
                *out_index = index;
                return entry;
            }
        }
        
        /* 继续探测下一个位置 */
        index = (index + 1) & (map->capacity - 1);
    }
}

/* ========== 扩容 ========== */

void xr_map_resize(XrMap *map, uint32_t new_capacity) {
    /* 分配新的条目数组 */
    XrMapEntry *new_entries = (XrMapEntry*)xmem_alloc(
        sizeof(XrMapEntry) * new_capacity
    );
    
    /* 初始化为空 */
    memset(new_entries, 0, sizeof(XrMapEntry) * new_capacity);
    
    /* 保存旧数据 */
    XrMapEntry *old_entries = map->entries;
    uint32_t old_capacity = map->capacity;
    
    /* 更新Map */
    map->entries = new_entries;
    map->capacity = new_capacity;
    map->count = 0;  /* 重新计数 */
    
    /* 重新插入所有有效条目（清理墓碑） */
    if (old_entries != NULL) {
        for (uint32_t i = 0; i < old_capacity; i++) {
            XrMapEntry *old_entry = &old_entries[i];
            
            if (entry_is_valid(old_entry)) {
                /* 在新表中查找位置 */
                uint32_t hash = xr_hash_value(old_entry->key);
                uint8_t sh = xr_short_hash(hash);
                uint32_t index = hash & (new_capacity - 1);
                
                /* 线性探测找到空位 */
                while (new_entries[index].state != XR_MAP_EMPTY) {
                    index = (index + 1) & (new_capacity - 1);
                }
                
                /* 插入 */
                new_entries[index].state = sh;
                new_entries[index].key = old_entry->key;
                new_entries[index].value = old_entry->value;
                map->count++;
            }
        }
        
        /* 释放旧数组 */
        xmem_free(old_entries);
    }
}

/* ========== 基础操作 ========== */

void xr_map_set(XrMap *map, XrValue key, XrValue value) {
    /* 检查是否需要扩容 */
    if (map->count + 1 > map->capacity * XR_MAP_LOAD_FACTOR) {
        uint32_t new_cap = next_capacity(map->capacity);
        xr_map_resize(map, new_cap);
    }
    
    /* 查找或插入位置 */
    uint32_t index;
    XrMapEntry *entry = xr_map_find_entry(map, key, &index);
    
    if (entry == NULL) {
        /* 新键：插入 */
        uint32_t hash = xr_hash_value(key);
        uint8_t sh = xr_short_hash(hash);
        
        map->entries[index].state = sh;
        map->entries[index].key = key;
        map->entries[index].value = value;
        map->count++;
    } else {
        /* 已存在：更新值 */
        entry->value = value;
    }
}

XrValue xr_map_get(XrMap *map, XrValue key, bool *found) {
    uint32_t index;
    XrMapEntry *entry = xr_map_find_entry(map, key, &index);
    
    if (entry != NULL) {
        if (found) *found = true;
        return entry->value;
    } else {
        if (found) *found = false;
        return xr_null();
    }
}

bool xr_map_has(XrMap *map, XrValue key) {
    uint32_t index;
    XrMapEntry *entry = xr_map_find_entry(map, key, &index);
    return entry != NULL;
}

bool xr_map_delete(XrMap *map, XrValue key) {
    uint32_t index;
    XrMapEntry *entry = xr_map_find_entry(map, key, &index);
    
    if (entry != NULL) {
        /* 标记为墓碑 */
        entry->state = XR_MAP_TOMBSTONE;
        entry->key = xr_null();
        entry->value = xr_null();
        map->count--;
        return true;
    }
    
    return false;
}

void xr_map_clear(XrMap *map) {
    if (map->entries != NULL) {
        /* 清空所有条目 */
        memset(map->entries, 0, sizeof(XrMapEntry) * map->capacity);
    }
    map->count = 0;
}

uint32_t xr_map_size(XrMap *map) {
    return map->count;
}

/* ========== Map迭代方法 ========== */

/*
** 获取所有键
** 返回包含所有键的数组
*/
struct XrArray* xr_map_keys(XrMap *map) {
    XrArray *keys = xr_array_new();
    
    /* 遍历所有有效条目 */
    for (uint32_t i = 0; i < map->capacity; i++) {
        if (entry_is_valid(&map->entries[i])) {
            xr_array_push(keys, map->entries[i].key);
        }
    }
    
    return keys;
}

/*
** 获取所有值
** 返回包含所有值的数组
*/
struct XrArray* xr_map_values(XrMap *map) {
    XrArray *values = xr_array_new();
    
    /* 遍历所有有效条目 */
    for (uint32_t i = 0; i < map->capacity; i++) {
        if (entry_is_valid(&map->entries[i])) {
            xr_array_push(values, map->entries[i].value);
        }
    }
    
    return values;
}

/*
** 获取所有键值对
** 返回包含所有[key, value]数组的数组
*/
struct XrArray* xr_map_entries(XrMap *map) {
    XrArray *entries = xr_array_new();
    
    /* 遍历所有有效条目 */
    for (uint32_t i = 0; i < map->capacity; i++) {
        if (entry_is_valid(&map->entries[i])) {
            /* 创建[key, value]数组 */
            XrArray *pair = xr_array_new();
            xr_array_push(pair, map->entries[i].key);
            xr_array_push(pair, map->entries[i].value);
            
            /* 包装成XrValue并添加到结果 */
            extern XrValue xr_value_from_array(struct XrArray *arr);
            xr_array_push(entries, xr_value_from_array(pair));
        }
    }
    
    return entries;
}


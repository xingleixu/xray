/* xmap.h - Map字典数据结构
 * 
 * 第11阶段 - Map字典实现
 * 
 * 设计：
 *   - 开放寻址（Open Addressing）
 *   - 线性探测（Linear Probing）
 *   - 墓碑机制（Tombstone）
 *   - 短哈希优化（Julia风格）
 *   - 小Map线性扫描（Crystal风格）
 * 
 * 特性：
 *   - O(1) 平均时间复杂度
 *   - 75% 负载因子
 *   - 容量为2的幂（位运算优化）
 *   - 预留WeakMap扩展接口
 */

#ifndef XMAP_H
#define XMAP_H

#include "xvalue.h"
#include "xgc.h"
#include <stdint.h>
#include <stdbool.h>

/* ========== Map条目 ========== */

/**
 * Map条目结构
 * 
 * state字段编码：
 *   - 0x00：空槽位
 *   - 0x7F：墓碑（已删除）
 *   - 0x80-0xFF：有效条目（bit7=1，低7位=哈希前缀）
 */
typedef struct {
    XrValue key;      /* 键 */
    XrValue value;    /* 值 */
    uint8_t state;    /* 状态/哈希前缀 */
} XrMapEntry;

/* 状态常量 */
#define XR_MAP_EMPTY     0x00  /* 空槽位 */
#define XR_MAP_TOMBSTONE 0x7F  /* 墓碑 */
#define XR_MAP_VALID     0x80  /* 有效条目（bit 7 = 1） */

/* ========== Map对象 ========== */

/**
 * Map对象结构
 */
typedef struct {
    GCHeader gc;           /* GC头（继承自所有堆对象） */
    uint32_t capacity;     /* 容量（2的幂） */
    uint32_t count;        /* 实际元素数量 */
    XrMapEntry *entries;   /* 条目数组 */
    uint8_t flags;         /* 标志位（预留WeakMap） */
    void *gc_data;         /* GC数据（预留） */
} XrMap;

/* 标志位定义（预留） */
#define XR_MAP_FLAG_WEAK     0x01  /* WeakMap标志（未来实现） */

/* ========== Map参数 ========== */

/* 最小容量 */
#define XR_MAP_MIN_CAPACITY 8

/* 负载因子（75%） */
#define XR_MAP_LOAD_FACTOR 0.75

/* 增长因子 */
#define XR_MAP_GROW_FACTOR 2

/* 小Map阈值（≤8个元素时线性扫描，不计算哈希） */
#define XR_MAP_SMALL_SIZE 8

/* ========== Map基础操作 ========== */

/**
 * 创建新的Map
 * 
 * @return 新创建的Map对象
 */
XrMap* xr_map_new(void);

/**
 * 释放Map
 * 
 * @param map 要释放的Map
 */
void xr_map_free(XrMap *map);

/**
 * 设置键值对
 * 如果键已存在则更新，否则插入新条目
 * 
 * @param map Map对象
 * @param key 键
 * @param value 值
 */
void xr_map_set(XrMap *map, XrValue key, XrValue value);

/**
 * 获取键对应的值
 * 
 * @param map Map对象
 * @param key 键
 * @param found 输出参数，是否找到键（可为NULL）
 * @return 键对应的值，如果不存在则返回null
 */
XrValue xr_map_get(XrMap *map, XrValue key, bool *found);

/**
 * 检查键是否存在
 * 
 * @param map Map对象
 * @param key 键
 * @return true如果键存在，false否则
 */
bool xr_map_has(XrMap *map, XrValue key);

/**
 * 删除键值对
 * 
 * @param map Map对象
 * @param key 键
 * @return true如果成功删除，false如果键不存在
 */
bool xr_map_delete(XrMap *map, XrValue key);

/**
 * 清空Map
 * 
 * @param map Map对象
 */
void xr_map_clear(XrMap *map);

/**
 * 获取Map大小
 * 
 * @param map Map对象
 * @return 元素数量
 */
uint32_t xr_map_size(XrMap *map);

/* ========== Map迭代方法 ========== */

/**
 * 获取所有键（返回数组）
 * 
 * @param map Map对象
 * @return 包含所有键的数组
 */
struct XrArray* xr_map_keys(XrMap *map);

/**
 * 获取所有值（返回数组）
 * 
 * @param map Map对象
 * @return 包含所有值的数组
 */
struct XrArray* xr_map_values(XrMap *map);

/**
 * 获取所有键值对（返回数组，每个元素是[key, value]数组）
 * 
 * @param map Map对象
 * @return 包含所有键值对的数组
 */
struct XrArray* xr_map_entries(XrMap *map);

/* ========== 内部函数（供实现使用） ========== */

/**
 * 查找条目
 * 
 * @param map Map对象
 * @param key 键
 * @param out_index 输出参数，找到的索引或可插入的索引
 * @return 找到的条目，如果不存在则返回NULL
 */
XrMapEntry* xr_map_find_entry(XrMap *map, XrValue key, uint32_t *out_index);

/**
 * 扩容Map
 * 
 * @param map Map对象
 * @param new_capacity 新容量
 */
void xr_map_resize(XrMap *map, uint32_t new_capacity);

/**
 * 小Map线性扫描查找
 * 
 * @param map Map对象
 * @param key 键
 * @param out_index 输出参数
 * @return 找到的条目或NULL
 */
XrMapEntry* xr_map_find_linear(XrMap *map, XrValue key, uint32_t *out_index);

#endif /* XMAP_H */


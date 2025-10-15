/* xhash.h - 哈希函数模块
 * 
 * 第11阶段 - Map字典实现
 * 
 * 功能：
 *   - 为不同类型的值提供高效的哈希函数
 *   - 支持类型特化优化（整数、浮点数、字符串）
 *   - 短哈希优化（Julia风格）
 * 
 * 设计参考：
 *   - Wren: Thomas Wang整数哈希
 *   - Julia: 短哈希前缀加速
 *   - Lua: 类型特化策略
 */

#ifndef XHASH_H
#define XHASH_H

#include "xvalue.h"
#include <stdint.h>

/* ========== 哈希函数 ========== */

/**
 * 计算值的32位哈希
 * 
 * @param val 要哈希的值
 * @return 32位哈希值
 * 
 * 说明：
 *   - 针对不同类型使用优化的哈希算法
 *   - 保证哈希值不为0（0用于墓碑标记）
 */
uint32_t xr_hash_value(XrValue val);

/**
 * 计算整数的哈希
 * 使用FNV-1a算法
 */
uint32_t xr_hash_int(xr_Integer val);

/**
 * 计算浮点数的哈希
 * 使用位模式哈希
 */
uint32_t xr_hash_float(xr_Number val);

/**
 * 计算字符串的哈希
 * 直接使用缓存的哈希值
 */
uint32_t xr_hash_string(XrString *str);

/**
 * 计算布尔值的哈希
 */
uint32_t xr_hash_bool(int val);

/**
 * 提取短哈希（7位前缀）
 * Julia优化：用于快速排除不匹配的键
 * 
 * @param hash 完整的32位哈希
 * @return 8位短哈希（最高位为1，低7位为哈希前缀）
 */
uint8_t xr_short_hash(uint32_t hash);

/**
 * 比较两个值是否相等（用于Map键比较）
 * 
 * @param a 第一个值
 * @param b 第二个值
 * @return true如果相等，false如果不相等
 * 
 * 说明：
 *   - 类型必须相同
 *   - 字符串使用内容比较
 *   - 浮点数使用位精确比较
 */
bool xr_map_keys_equal(XrValue a, XrValue b);

/* ========== 常量定义 ========== */

/* 哈希魔数 - FNV-1a */
#define XR_FNV_OFFSET_BASIS 2166136261u
#define XR_FNV_PRIME        16777619u

/* 短哈希标记位 */
#define XR_SHORT_HASH_VALID 0x80  /* 最高位为1表示有效条目 */

#endif /* XHASH_H */


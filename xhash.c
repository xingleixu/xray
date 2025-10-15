/* xhash.c - 哈希函数实现
 * 
 * 第11阶段 - Map字典实现
 */

#include "xhash.h"
#include "xstring.h"
#include <string.h>
#include <math.h>

/* ========== 整数哈希（FNV-1a） ========== */

uint32_t xr_hash_int(xr_Integer val) {
    uint32_t hash = XR_FNV_OFFSET_BASIS;
    
    /* 将整数按字节哈希 */
    uint8_t *bytes = (uint8_t*)&val;
    for (size_t i = 0; i < sizeof(val); i++) {
        hash ^= bytes[i];
        hash *= XR_FNV_PRIME;
    }
    
    /* 确保哈希值不为0（0用作墓碑标记） */
    return hash == 0 ? 1 : hash;
}

/* ========== 浮点数哈希 ========== */

uint32_t xr_hash_float(xr_Number val) {
    /* 特殊值处理 */
    if (val == 0.0) {
        /* 0.0 和 -0.0 视为相同 */
        return xr_hash_int(0);
    }
    
    if (isnan(val)) {
        /* 所有NaN视为相同 */
        return xr_hash_int(1);
    }
    
    if (isinf(val)) {
        /* +Inf 和 -Inf 不同 */
        return xr_hash_int(val > 0 ? 2 : 3);
    }
    
    /* 正常值：转为位模式并哈希 */
    uint64_t bits;
    memcpy(&bits, &val, sizeof(bits));
    
    /* 混合高低32位 */
    uint32_t hash = (uint32_t)(bits ^ (bits >> 32));
    
    return hash == 0 ? 1 : hash;
}

/* ========== 字符串哈希 ========== */

uint32_t xr_hash_string(XrString *str) {
    /* 字符串对象已经缓存了哈希值 */
    return str->hash == 0 ? 1 : str->hash;
}

/* ========== 布尔值哈希 ========== */

uint32_t xr_hash_bool(int val) {
    /* true 和 false 使用固定值 */
    return val ? 5 : 4;
}

/* ========== 统一哈希接口 ========== */

uint32_t xr_hash_value(XrValue val) {
    switch (val.type) {
        case XR_TNULL:
            /* null固定哈希值 */
            return 6;
            
        case XR_TBOOL:
            return xr_hash_bool(XR_TO_BOOL(val));
            
        case XR_TINT:
            return xr_hash_int(XR_TO_INT(val));
            
        case XR_TFLOAT:
            return xr_hash_float(XR_TO_FLOAT(val));
            
        case XR_TSTRING:
            return xr_hash_string(xr_tostring(val));
            
        default:
            /* 其他类型暂不支持作为Map键 */
            return 0;
    }
}

/* ========== 短哈希提取 ========== */

uint8_t xr_short_hash(uint32_t hash) {
    /* 提取高7位，最高位设为1（表示有效条目） */
    return (uint8_t)((hash >> 25) | XR_SHORT_HASH_VALID);
}

/* ========== 值相等比较 ========== */

bool xr_map_keys_equal(XrValue a, XrValue b) {
    /* 类型必须相同 */
    if (a.type != b.type) {
        return false;
    }
    
    switch (a.type) {
        case XR_TNULL:
            /* null总是等于null */
            return true;
            
        case XR_TBOOL:
            return XR_TO_BOOL(a) == XR_TO_BOOL(b);
            
        case XR_TINT:
            return XR_TO_INT(a) == XR_TO_INT(b);
            
        case XR_TFLOAT: {
            /* 浮点数使用位精确比较 */
            double fa = XR_TO_FLOAT(a);
            double fb = XR_TO_FLOAT(b);
            
            /* NaN != NaN */
            if (isnan(fa) || isnan(fb)) {
                return false;
            }
            
            /* 0.0 == -0.0 */
            if (fa == 0.0 && fb == 0.0) {
                return true;
            }
            
            return fa == fb;
        }
            
        case XR_TSTRING: {
            /* 字符串内容比较 */
            XrString *sa = xr_tostring(a);
            XrString *sb = xr_tostring(b);
            
            /* 先比较长度 */
            if (sa->length != sb->length) {
                return false;
            }
            
            /* 再比较哈希（快速排除） */
            if (sa->hash != sb->hash) {
                return false;
            }
            
            /* 最后比较内容 */
            return memcmp(sa->chars, sb->chars, sa->length) == 0;
        }
            
        default:
            /* 其他类型暂不支持 */
            return false;
    }
}


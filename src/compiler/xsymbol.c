/*
** xsymbol.c
** Xray Symbol系统实现
** 
** v0.20.0：Phase 2 - 方法索引优化
*/

#include "xsymbol.h"
#include "../utils/xhashmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* 临时使用标准malloc/free，后续改用统一的内存管理 */
#define xmem_alloc(size) malloc(size)
#define xmem_free(ptr) free(ptr)
#define xmem_realloc(ptr, old_size, new_size) realloc(ptr, new_size)

/* ========== 全局Symbol表 ========== */

SymbolTable *global_method_symbols = NULL;

/* ========== 内部辅助函数 ========== */

/*
** 复制字符串
*/
static char* str_dup(const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char *copy = (char*)xmem_alloc(len + 1);
    memcpy(copy, str, len + 1);
    return copy;
}

/*
** 扩展Symbol表容量
*/
static void expand_capacity(SymbolTable *table) {
    int new_capacity = table->capacity == 0 ? 16 : table->capacity * 2;
    
    table->entries = (SymbolEntry*)xmem_realloc(
        table->entries,
        sizeof(SymbolEntry) * table->capacity,
        sizeof(SymbolEntry) * new_capacity
    );
    
    table->capacity = new_capacity;
}

/* ========== Symbol表操作实现 ========== */

/*
** 创建新的Symbol表
*/
SymbolTable* symbol_table_new(void) {
    SymbolTable *table = (SymbolTable*)xmem_alloc(sizeof(SymbolTable));
    
    table->entries = NULL;
    table->count = 0;
    table->capacity = 0;
    table->name_to_symbol = xr_hashmap_new();
    
    return table;
}

/*
** 释放Symbol表
*/
void symbol_table_free(SymbolTable *table) {
    if (!table) return;
    
    /* 释放所有方法名字符串 */
    for (int i = 0; i < table->count; i++) {
        if (table->entries[i].name) {
            xmem_free(table->entries[i].name);
        }
    }
    
    /* 释放entries数组 */
    if (table->entries) {
        xmem_free(table->entries);
    }
    
    /* 释放哈希表 */
    xr_hashmap_free(table->name_to_symbol);
    
    /* 释放table本身 */
    xmem_free(table);
}

/*
** 获取或创建Symbol
*/
int symbol_get_or_create(SymbolTable *table, const char *name) {
    assert(table != NULL);
    assert(name != NULL);
    
    /* 1. 查找已存在的symbol */
    void *existing = xr_hashmap_get(table->name_to_symbol, name);
    if (existing != NULL) {
        /* 找到了，返回已有的symbol */
        return *(int*)existing;
    }
    
    /* 2. 分配新symbol */
    int new_symbol = table->count;
    
    /* 3. 扩展容量（如果需要）*/
    if (table->count >= table->capacity) {
        expand_capacity(table);
    }
    
    /* 4. 记录新symbol */
    table->entries[new_symbol].symbol = new_symbol;
    table->entries[new_symbol].name = str_dup(name);
    
    /* 5. 添加到哈希表（name → symbol）*/
    int *symbol_ptr = (int*)xmem_alloc(sizeof(int));
    *symbol_ptr = new_symbol;
    xr_hashmap_set(table->name_to_symbol, name, symbol_ptr);
    
    /* 6. 增加计数 */
    table->count++;
    
    return new_symbol;
}

/*
** 查找Symbol（不创建）
*/
int symbol_lookup(SymbolTable *table, const char *name) {
    if (!table || !name) return -1;
    
    void *result = xr_hashmap_get(table->name_to_symbol, name);
    if (result == NULL) {
        return -1;  /* 未找到 */
    }
    
    return *(int*)result;
}

/*
** 获取Symbol对应的名字
*/
const char* symbol_get_name(SymbolTable *table, int symbol) {
    if (!table || symbol < 0 || symbol >= table->count) {
        return NULL;
    }
    
    return table->entries[symbol].name;
}

/* ========== 初始化与清理 ========== */

/*
** 初始化预定义Symbol
*/
static void init_predefined_symbols(SymbolTable *table) {
    /* 预定义常用方法Symbol */
    assert(symbol_get_or_create(table, "constructor") == SYMBOL_CONSTRUCTOR);
    assert(symbol_get_or_create(table, "toString") == SYMBOL_TOSTRING);
    assert(symbol_get_or_create(table, "equals") == SYMBOL_EQUALS);
    assert(symbol_get_or_create(table, "hashCode") == SYMBOL_HASHCODE);
    
    /* 预定义运算符Symbol */
    assert(symbol_get_or_create(table, "+") == SYMBOL_OP_ADD);
    assert(symbol_get_or_create(table, "-") == SYMBOL_OP_SUB);
    assert(symbol_get_or_create(table, "*") == SYMBOL_OP_MUL);
    assert(symbol_get_or_create(table, "/") == SYMBOL_OP_DIV);
    assert(symbol_get_or_create(table, "%") == SYMBOL_OP_MOD);
    assert(symbol_get_or_create(table, "==") == SYMBOL_OP_EQ);
    assert(symbol_get_or_create(table, "!=") == SYMBOL_OP_NE);
    assert(symbol_get_or_create(table, "<") == SYMBOL_OP_LT);
    assert(symbol_get_or_create(table, "<=") == SYMBOL_OP_LE);
    assert(symbol_get_or_create(table, ">") == SYMBOL_OP_GT);
    assert(symbol_get_or_create(table, ">=") == SYMBOL_OP_GE);
    
    /* 验证预定义数量 */
    assert(table->count == SYMBOL_PREDEFINED_COUNT);
}

/*
** 初始化全局Symbol表
*/
void init_global_symbols(void) {
    if (global_method_symbols != NULL) {
        /* 已初始化 */
        return;
    }
    
    global_method_symbols = symbol_table_new();
    
    /* 初始化预定义Symbol */
    init_predefined_symbols(global_method_symbols);
}

/*
** 清理全局Symbol表
*/
void cleanup_global_symbols(void) {
    if (global_method_symbols) {
        symbol_table_free(global_method_symbols);
        global_method_symbols = NULL;
    }
}

/* ========== 调试工具实现 ========== */

/*
** 打印Symbol表（调试用）
*/
void symbol_table_print(SymbolTable *table) {
    if (!table) {
        printf("null symbol table\n");
        return;
    }
    
    printf("Symbol Table (%d symbols):\n", table->count);
    printf("  %-6s  %s\n", "Symbol", "Name");
    printf("  %-6s  %s\n", "------", "----");
    
    for (int i = 0; i < table->count; i++) {
        printf("  %-6d  %s", table->entries[i].symbol, table->entries[i].name);
        
        /* 标记预定义Symbol */
        if (i < SYMBOL_PREDEFINED_COUNT) {
            printf("  (predefined)");
        }
        
        printf("\n");
    }
}

/*
** 获取Symbol统计信息
*/
void symbol_table_stats(SymbolTable *table, int *count, int *capacity) {
    if (!table) {
        *count = 0;
        *capacity = 0;
        return;
    }
    
    *count = table->count;
    *capacity = table->capacity;
}

/* ========== 便利函数 ========== */

/*
** 获取常用方法的Symbol
** 
** 说明：
** - 直接返回预定义的symbol常量
** - 无需查找，最快速度
*/
int symbol_for_constructor(void) {
    return SYMBOL_CONSTRUCTOR;
}

int symbol_for_tostring(void) {
    return SYMBOL_TOSTRING;
}

int symbol_for_equals(void) {
    return SYMBOL_EQUALS;
}

/* ========== 使用示例 ========== */

#if 0
/*
** 编译器使用示例
*/
void example_compiler_usage(void) {
    /* 1. 初始化（程序启动时）*/
    init_global_symbols();
    
    /* 2. 编译方法定义时 */
    const char *method_name = "add";
    int symbol = symbol_get_or_create(global_method_symbols, method_name);
    // symbol = 15（假设）
    
    /* 生成字节码 */
    // emit(OP_DEFINE_METHOD, symbol);
    
    /* 3. 编译方法调用时 */
    const char *call_name = "add";
    int call_symbol = symbol_get_or_create(global_method_symbols, call_name);
    // call_symbol = 15（同一个symbol）
    
    /* 生成字节码 */
    // emit(OP_INVOKE_SYMBOL, call_symbol, arg_count);
    
    /* 4. 程序退出时 */
    cleanup_global_symbols();
}

/*
** VM使用示例
*/
void example_vm_usage(void) {
    /* VM执行 OP_INVOKE_SYMBOL */
    int symbol = read_short();  // 从字节码读取symbol
    
    /* 通过symbol直接索引方法 - O(1)! */
    XrMethod *method = instance->klass->methods[symbol];
    
    /* 调用方法 */
    // call_method(method);
}
#endif



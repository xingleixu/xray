/*
** xsymbol.h
** Xray Symbol系统 - 方法名到整数的映射
** 
** v0.20.0：Phase 2 - 方法索引优化
** 参考：Wren的Symbol机制
** 
** 设计说明：
** 1. 全局Symbol表：方法名 → 唯一整数symbol
** 2. 编译期分配：解析到方法定义时分配symbol
** 3. 运行期使用：通过symbol直接索引方法数组
** 4. 性能提升：哈希查找(~16周期) → 数组访问(~1周期)
*/

#ifndef xsymbol_h
#define xsymbol_h

#include <stdbool.h>
#include "../utils/xhashmap.h"

/* ========== Symbol表数据结构 ========== */

/*
** Symbol条目
** 
** 记录symbol和对应的方法名
*/
typedef struct {
    int symbol;           /* Symbol编号（从0开始）*/
    char *name;           /* 方法名："add", "toString"等 */
} SymbolEntry;

/*
** Symbol表
** 
** 双向映射：
** - name → symbol（用于编译期查找/分配）
** - symbol → name（用于运行期调试/错误信息）
*/
typedef struct {
    SymbolEntry *entries;      /* symbol → name 数组 */
    int count;                 /* 当前symbol数量 */
    int capacity;              /* entries数组容量 */
    
    XrHashMap *name_to_symbol; /* name → symbol 哈希表（快速查找）*/
} SymbolTable;

/* ========== 全局Symbol表 ========== */

/*
** 全局Symbol表（编译器使用）
** 
** 说明：
** - 整个程序只有一个全局Symbol表
** - 所有方法名共享这个表
** - 编译期分配，运行期只读
*/
extern SymbolTable *global_method_symbols;

/* ========== Symbol表操作 ========== */

/*
** 创建新的Symbol表
** 
** @return  新的Symbol表
*/
SymbolTable* symbol_table_new(void);

/*
** 释放Symbol表
** 
** @param table  Symbol表
*/
void symbol_table_free(SymbolTable *table);

/*
** 获取或创建Symbol
** 
** @param table  Symbol表
** @param name   方法名
** @return       symbol编号（如果是新方法名，分配新symbol）
** 
** 说明：
** - 如果name已存在，返回已有的symbol
** - 如果name是新的，分配新symbol并记录映射
** - 这是编译期使用的主要接口
*/
int symbol_get_or_create(SymbolTable *table, const char *name);

/*
** 查找Symbol（不创建）
** 
** @param table  Symbol表
** @param name   方法名
** @return       symbol编号（-1表示未找到）
** 
** 说明：
** - 仅查找，不分配新symbol
** - 用于验证方法是否已定义
*/
int symbol_lookup(SymbolTable *table, const char *name);

/*
** 获取Symbol对应的名字
** 
** @param table  Symbol表
** @param symbol symbol编号
** @return       方法名（NULL表示无效symbol）
** 
** 说明：
** - 运行期用于错误信息
** - 调试时查看symbol含义
*/
const char* symbol_get_name(SymbolTable *table, int symbol);

/* ========== 初始化与清理 ========== */

/*
** 初始化全局Symbol表
** 
** 说明：
** - 程序启动时调用一次
** - 初始化 global_method_symbols
*/
void init_global_symbols(void);

/*
** 清理全局Symbol表
** 
** 说明：
** - 程序退出时调用
** - 释放 global_method_symbols
*/
void cleanup_global_symbols(void);

/* ========== 预定义的常用Symbol ========== */

/*
** 预定义的Symbol常量
** 
** 说明：
** - 常用方法预分配固定symbol
** - 提高编译速度
** - 便于调试
*/
#define SYMBOL_CONSTRUCTOR  0   /* "constructor" */
#define SYMBOL_TOSTRING     1   /* "toString" */
#define SYMBOL_EQUALS       2   /* "equals" */
#define SYMBOL_HASHCODE     3   /* "hashCode" */

/* 运算符Symbol */
#define SYMBOL_OP_ADD       4   /* "+" */
#define SYMBOL_OP_SUB       5   /* "-" */
#define SYMBOL_OP_MUL       6   /* "*" */
#define SYMBOL_OP_DIV       7   /* "/" */
#define SYMBOL_OP_MOD       8   /* "%" */
#define SYMBOL_OP_EQ        9   /* "==" */
#define SYMBOL_OP_NE        10  /* "!=" */
#define SYMBOL_OP_LT        11  /* "<" */
#define SYMBOL_OP_LE        12  /* "<=" */
#define SYMBOL_OP_GT        13  /* ">" */
#define SYMBOL_OP_GE        14  /* ">=" */

#define SYMBOL_PREDEFINED_COUNT 15  /* 预定义Symbol数量 */

/* ========== 调试工具 ========== */

/*
** 打印Symbol表（调试用）
** 
** @param table  Symbol表
*/
void symbol_table_print(SymbolTable *table);

/*
** 获取Symbol统计信息
** 
** @param table  Symbol表
** @param count  输出：symbol数量
** @param capacity 输出：表容量
*/
void symbol_table_stats(SymbolTable *table, int *count, int *capacity);

#endif /* xsymbol_h */


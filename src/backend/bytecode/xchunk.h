/*
** xchunk.h
** Xray 字节码块 - 寄存器VM指令系统
** 
** v0.13.0: 32位寄存器指令，参考Lua 5.4设计
** 创建日期: 2025-10-15
*/

#ifndef xchunk_h
#define xchunk_h

#include "xray.h"
#include "xvalue.h"
#include <stdint.h>

/* 前向声明 */
typedef struct XrString XrString;

/* ========== 32位指令类型 ========== */

typedef uint32_t Instruction;

/* ========== 指令格式定义 ========== */

/*
** 指令格式（参考Lua 5.4）
**
**  31           23           15            7    0
**  +------------+------------+------------+----+
**  |      C     |      B     |      A     | OP | iABC
**  +------------+------------+------------+----+
**  |            Bx           |      A     | OP | iABx
**  +------------+------------+------------+----+
**  |            sBx          |      A     | OP | iAsBx
**  +------------+------------+------------+----+
**  |            Ax                        | OP | iAx
**  +------------+------------+------------+----+
**  |            sJ                        | OP | isJ
**  +------------+------------+------------+----+
**
** 字段大小:
**   OP: 8位 (256个操作码)
**   A:  8位 (256个寄存器)
**   B:  8位
**   C:  8位
**   Bx: 16位 (65536个常量)
**   sBx: 16位有符号 (-32768 到 32767)
**   Ax: 24位
**   sJ: 24位有符号 (跳转偏移)
*/

/* 参数最大值 */
#define SIZE_OP     8
#define SIZE_A      8
#define SIZE_B      8
#define SIZE_C      8
#define SIZE_Bx     16
#define SIZE_Ax     24

#define MAXARG_A    ((1 << SIZE_A) - 1)
#define MAXARG_B    ((1 << SIZE_B) - 1)
#define MAXARG_C    ((1 << SIZE_C) - 1)
#define MAXARG_Bx   ((1 << SIZE_Bx) - 1)
#define MAXARG_sBx  (MAXARG_Bx >> 1)
#define MAXARG_Ax   ((1 << SIZE_Ax) - 1)
#define MAXARG_sJ   ((1 << (SIZE_Ax - 1)) - 1)

/* ========== 操作码枚举 ========== */

typedef enum {
    /* === 基础操作（7个）=== */
    OP_MOVE,        /* R[A] = R[B] */
    OP_LOADI,       /* R[A] = sBx (整数立即数) */
    OP_LOADF,       /* R[A] = (float)sBx */
    OP_LOADK,       /* R[A] = K[Bx] (常量) */
    OP_LOADNIL,     /* R[A] = ... = R[A+B] = nil */
    OP_LOADTRUE,    /* R[A] = true */
    OP_LOADFALSE,   /* R[A] = false */
    
    /* === 算术运算（15个）=== */
    OP_ADD,         /* R[A] = R[B] + R[C] */
    OP_ADDI,        /* R[A] = R[B] + sC (立即数优化) */
    OP_ADDK,        /* R[A] = R[B] + K[C] (常量优化) */
    OP_SUB,         /* R[A] = R[B] - R[C] */
    OP_SUBI,        /* R[A] = R[B] - sC */
    OP_SUBK,        /* R[A] = R[B] - K[C] */
    OP_MUL,         /* R[A] = R[B] * R[C] */
    OP_MULI,        /* R[A] = R[B] * sC */
    OP_MULK,        /* R[A] = R[B] * K[C] */
    OP_DIV,         /* R[A] = R[B] / R[C] */
    OP_DIVK,        /* R[A] = R[B] / K[C] */
    OP_MOD,         /* R[A] = R[B] % R[C] */
    OP_MODK,        /* R[A] = R[B] % K[C] */
    OP_UNM,         /* R[A] = -R[B] (一元取负) */
    OP_NOT,         /* R[A] = not R[B] (逻辑非) */
    
    /* === 比较运算（11个）=== */
    OP_EQ,          /* if (R[A] == R[B]) != k then PC++ */
    OP_EQK,         /* if (R[A] == K[B]) != k then PC++ */
    OP_EQI,         /* if (R[A] == sB) != k then PC++ */
    OP_LT,          /* if (R[A] < R[B]) != k then PC++ */
    OP_LTI,         /* if (R[A] < sB) != k then PC++ */
    OP_LE,          /* if (R[A] <= R[B]) != k then PC++ */
    OP_LEI,         /* if (R[A] <= sB) != k then PC++ */
    OP_GT,          /* if (R[A] > R[B]) != k then PC++ */
    OP_GTI,         /* if (R[A] > sB) != k then PC++ */
    OP_GE,          /* if (R[A] >= R[B]) != k then PC++ */
    OP_GEI,         /* if (R[A] >= sB) != k then PC++ */
    
    /* === 控制流（7个）=== */
    OP_JMP,         /* PC += sJ */
    OP_TEST,        /* if (R[A]) != k then PC++ */
    OP_TESTSET,     /* if (R[B]) != k then PC++ else R[A] = R[B] */
    OP_CALL,        /* R[A]...R[A+C-2] = R[A](R[A+1]...R[A+B-1]) */
    OP_CALLSELF,    /* R[A]...R[A+C-2] = self(R[A+1]...R[A+B-1]) - 递归调用优化 */
    OP_TAILCALL,    /* R[A](R[A+1]...R[A+B-1]) - 尾调用优化 */
    OP_RETURN,      /* return R[A]...R[A+B-2] */
    
    /* === 表操作（8个）=== */
    OP_NEWTABLE,    /* R[A] = {} (创建表/数组) */
    OP_GETTABLE,    /* R[A] = R[B][R[C]] */
    OP_GETI,        /* R[A] = R[B][C] (整数索引优化) */
    OP_GETFIELD,    /* R[A] = R[B][K[C]:string] (字段访问优化) */
    OP_SETTABLE,    /* R[A][R[B]] = R[C] */
    OP_SETI,        /* R[A][B] = R[C] */
    OP_SETFIELD,    /* R[A][K[B]:string] = R[C] */
    OP_SETLIST,     /* R[A][i] = R[A+i], 1 <= i <= B */
    
    /* === 闭包（4个）=== */
    OP_CLOSURE,     /* R[A] = closure(PROTO[Bx]) */
    OP_GETUPVAL,    /* R[A] = UpValue[B] */
    OP_SETUPVAL,    /* UpValue[B] = R[A] */
    OP_CLOSE,       /* close upvalues >= R[A] */
    
    /* === OOP（9个）=== */
    OP_CLASS,       /* R[A] = new Class */
    OP_ADDFIELD,    /* R[A].add_field(K[B], K[C]) (添加字段定义) */
    OP_INHERIT,     /* R[A].super = R[B] */
    OP_GETPROP,     /* R[A] = R[B].K[C] */
    OP_SETPROP,     /* R[A].K[B] = R[C] */
    OP_GETSUPER,    /* R[A] = R[B].super.K[C] */
    OP_INVOKE,      /* R[A] = R[B]:method(...) (优化的方法调用) */
    OP_SUPERINVOKE, /* super.method(...) */
    OP_METHOD,      /* R[A].K[B] = R[C] (定义方法) */
    
    /* === 全局变量（3个）=== */
    OP_GETGLOBAL,   /* R[A] = _G[K[Bx]] */
    OP_SETGLOBAL,   /* _G[K[Bx]] = R[A] */
    OP_DEFGLOBAL,   /* 定义全局变量 */
    
    /* === 内置函数（1个）=== */
    OP_PRINT,       /* print(R[A]) - 打印寄存器值 */
    
    /* === 占位符 === */
    OP_NOP,         /* 无操作 */
    
} OpCode;

/* 操作码数量 */
#define NUM_OPCODES (OP_NOP + 1)

/* ========== 指令编码/解码宏 ========== */

/* 获取操作码 */
#define GET_OPCODE(i)   ((OpCode)((i) & 0xFF))

/* 创建指令 */
#define CREATE_ABC(op, a, b, c) \
    ((Instruction)(((op) & 0xFF) | \
                   (((a) & 0xFF) << 8) | \
                   (((b) & 0xFF) << 16) | \
                   (((c) & 0xFF) << 24)))

#define CREATE_ABx(op, a, bx) \
    ((Instruction)(((op) & 0xFF) | \
                   (((a) & 0xFF) << 8) | \
                   (((bx) & 0xFFFF) << 16)))

#define CREATE_AsBx(op, a, sbx) \
    CREATE_ABx(op, a, (sbx) + MAXARG_sBx)

#define CREATE_Ax(op, ax) \
    ((Instruction)(((op) & 0xFF) | \
                   (((ax) & 0xFFFFFF) << 8)))

#define CREATE_sJ(op, sj) \
    CREATE_Ax(op, (sj) + MAXARG_sJ)

/* 提取参数 */
#define GETARG_A(i)    (((i) >> 8)  & 0xFF)
#define GETARG_B(i)    (((i) >> 16) & 0xFF)
#define GETARG_C(i)    (((i) >> 24) & 0xFF)
#define GETARG_sB(i)   ((int8_t)(GETARG_B(i)))  /* 有符号B参数 */
#define GETARG_sC(i)   ((int8_t)(GETARG_C(i)))  /* 有符号C参数 */
#define GETARG_Bx(i)   (((i) >> 16) & 0xFFFF)
#define GETARG_sBx(i)  ((int)(GETARG_Bx(i)) - MAXARG_sBx)
#define GETARG_Ax(i)   (((i) >> 8)  & 0xFFFFFF)
#define GETARG_sJ(i)   ((int)(GETARG_Ax(i)) - MAXARG_sJ)

/* 设置参数（修改指令） */
#define SETARG_A(i, v)  ((i) = ((i) & ~(0xFF << 8))  | (((v) & 0xFF) << 8))
#define SETARG_B(i, v)  ((i) = ((i) & ~(0xFF << 16)) | (((v) & 0xFF) << 16))
#define SETARG_C(i, v)  ((i) = ((i) & ~(0xFF << 24)) | (((v) & 0xFF) << 24))
#define SETARG_Bx(i, v) ((i) = ((i) & ~(0xFFFF << 16)) | (((v) & 0xFFFF) << 16))

/* ========== 常量表 ========== */

/* 常量数组 */
typedef struct {
    int count;          /* 当前常量数量 */
    int capacity;       /* 容量 */
    XrValue *values;    /* 常量值数组 */
} ValueArray;

/* 常量表操作 */
void xr_valuearray_init(ValueArray *array);
void xr_valuearray_free(ValueArray *array);
int xr_valuearray_add(ValueArray *array, XrValue value);

/* ========== 函数原型（Proto）========== */

/* Upvalue信息 */
typedef struct {
    uint8_t index;      /* upvalue索引 */
    uint8_t is_local;   /* 是否是局部变量（1）或外层upvalue（0） */
} UpvalInfo;

/* 函数原型（编译后的函数） */
typedef struct Proto {
    /* 字节码 */
    Instruction *code;      /* 字节码数组 */
    int sizecode;           /* 字节码数量 */
    int capacity_code;      /* 字节码容量 */
    
    /* 全局变量信息（Wren风格优化） */
    int num_globals;        /* 全局变量数量 */
    
    /* 常量表 */
    ValueArray constants;   /* 常量池 */
    
    /* 嵌套函数 */
    struct Proto **protos;  /* 嵌套函数原型数组 */
    int sizeprotos;         /* 嵌套函数数量 */
    int capacity_protos;    /* 嵌套函数容量 */
    
    /* Upvalue信息 */
    UpvalInfo *upvalues;    /* upvalue信息数组 */
    int sizeupvalues;       /* upvalue数量 */
    int capacity_upvalues;  /* upvalue容量 */
    
    /* 调试信息 */
    int *lineinfo;          /* 行号信息（与code数组对应） */
    int size_lineinfo;      /* 行号信息数量 */
    int capacity_lineinfo;  /* 行号信息容量 */
    
    /* 函数信息 */
    XrString *name;         /* 函数名 */
    int maxstacksize;       /* 最大栈（寄存器）大小 */
    int numparams;          /* 参数数量 */
    bool is_vararg;         /* 是否是可变参数函数 */
} Proto;

/* Proto操作 */
Proto *xr_bc_proto_new(void);
void xr_bc_proto_free(Proto *proto);

/* 字节码操作 */
void xr_bc_proto_write(Proto *proto, Instruction inst, int line);
int xr_bc_proto_add_constant(Proto *proto, XrValue value);
int xr_bc_proto_add_proto(Proto *proto, Proto *child);
int xr_bc_proto_add_upvalue(Proto *proto, uint8_t index, uint8_t is_local);

/* ========== 调试辅助 ========== */

/* 获取操作码名称 */
const char *xr_opcode_name(OpCode op);

#endif /* xchunk_h */


/*
** xchunk.c
** Xray 字节码块实现
*/

#include "xchunk.h"
#include "xstring.h"
#include "xmem.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========== 操作码名称表 ========== */

static const char *opcode_names[] = {
    /* 基础操作 */
    "MOVE", "LOADI", "LOADF", "LOADK", "LOADNIL", "LOADTRUE", "LOADFALSE",
    
    /* 算术运算 */
    "ADD", "ADDI", "ADDK", "SUB", "SUBI", "SUBK",
    "MUL", "MULI", "MULK", "DIV", "DIVK", "MOD", "MODK",
    "UNM", "NOT",
    
    /* 比较运算 */
    "EQ", "EQK", "EQI", "LT", "LTI", "LE", "LEI",
    "GT", "GTI", "GE", "GEI",
    
    /* 控制流 */
    "JMP", "TEST", "TESTSET", "CALL", "TAILCALL", "RETURN",
    
    /* 表操作 */
    "NEWTABLE", "GETTABLE", "GETI", "GETFIELD",
    "SETTABLE", "SETI", "SETFIELD", "SETLIST",
    
    /* 闭包 */
    "CLOSURE", "GETUPVAL", "SETUPVAL", "CLOSE",
    
    /* OOP */
    "CLASS", "INHERIT", "GETPROP", "SETPROP",
    "GETSUPER", "INVOKE", "SUPERINVOKE", "METHOD",
    
    /* 全局变量 */
    "GETGLOBAL", "SETGLOBAL", "DEFGLOBAL",
    
    /* 内置函数 */
    "PRINT",
    
    /* 占位符 */
    "NOP",
};

/*
** 获取操作码名称
*/
const char *xr_opcode_name(OpCode op) {
    if (op >= 0 && op < NUM_OPCODES) {
        return opcode_names[op];
    }
    return "UNKNOWN";
}

/* ========== 常量表操作 ========== */

/*
** 初始化常量数组
*/
void xr_valuearray_init(ValueArray *array) {
    array->count = 0;
    array->capacity = 0;
    array->values = NULL;
}

/*
** 释放常量数组
*/
void xr_valuearray_free(ValueArray *array) {
    if (array->values != NULL) {
        xr_free(array->values);
        array->values = NULL;
    }
    array->count = 0;
    array->capacity = 0;
}

/*
** 添加常量到数组
** 返回常量索引
*/
int xr_valuearray_add(ValueArray *array, XrValue value) {
    /* 检查是否已存在相同常量（去重） */
    for (int i = 0; i < array->count; i++) {
        /* TODO: 实现值比较函数 xr_value_equals() */
        /* 暂时简单比较 */
    }
    
    /* 扩容 */
    if (array->count >= array->capacity) {
        int old_capacity = array->capacity;
        array->capacity = XR_GROW_CAPACITY(old_capacity);
        array->values = XR_GROW_ARRAY(XrValue, array->values, 
                                      old_capacity, array->capacity);
    }
    
    /* 添加新常量 */
    array->values[array->count] = value;
    return array->count++;
}

/* ========== Proto操作 ========== */

/*
** 创建新的函数原型
*/
Proto *xr_bc_proto_new(void) {
    Proto *proto = (Proto *)xr_malloc(sizeof(Proto));
    if (proto == NULL) {
        return NULL;
    }
    
    /* 初始化字节码 */
    proto->code = NULL;
    proto->sizecode = 0;
    proto->capacity_code = 0;
    
    /* 初始化常量表 */
    xr_valuearray_init(&proto->constants);
    
    /* 初始化嵌套函数 */
    proto->protos = NULL;
    proto->sizeprotos = 0;
    proto->capacity_protos = 0;
    
    /* 初始化upvalue信息 */
    proto->upvalues = NULL;
    proto->sizeupvalues = 0;
    proto->capacity_upvalues = 0;
    
    /* 初始化调试信息 */
    proto->lineinfo = NULL;
    proto->size_lineinfo = 0;
    proto->capacity_lineinfo = 0;
    
    /* 初始化函数信息 */
    proto->name = NULL;
    proto->maxstacksize = 0;
    proto->numparams = 0;
    proto->is_vararg = false;
    
    return proto;
}

/*
** 释放函数原型
*/
void xr_bc_proto_free(Proto *proto) {
    if (proto == NULL) {
        return;
    }
    
    /* 释放字节码 */
    if (proto->code != NULL) {
        xr_free(proto->code);
        proto->code = NULL;
    }
    
    /* 释放常量表 */
    xr_valuearray_free(&proto->constants);
    
    /* 释放嵌套函数 */
    if (proto->protos != NULL) {
        for (int i = 0; i < proto->sizeprotos; i++) {
            xr_bc_proto_free(proto->protos[i]);
        }
        xr_free(proto->protos);
        proto->protos = NULL;
    }
    
    /* 释放upvalue信息 */
    if (proto->upvalues != NULL) {
        xr_free(proto->upvalues);
        proto->upvalues = NULL;
    }
    
    /* 释放行号信息 */
    if (proto->lineinfo != NULL) {
        xr_free(proto->lineinfo);
        proto->lineinfo = NULL;
    }
    
    /* 释放Proto本身 */
    xr_free(proto);
}

/*
** 写入一条指令
*/
void xr_bc_proto_write(Proto *proto, Instruction inst, int line) {
    /* 扩容字节码数组 */
    if (proto->sizecode >= proto->capacity_code) {
        int old_capacity = proto->capacity_code;
        proto->capacity_code = XR_GROW_CAPACITY(old_capacity);
        proto->code = XR_GROW_ARRAY(Instruction, proto->code,
                                    old_capacity, proto->capacity_code);
    }
    
    /* 写入指令 */
    proto->code[proto->sizecode] = inst;
    
    /* 扩容行号数组 */
    if (proto->size_lineinfo >= proto->capacity_lineinfo) {
        int old_capacity = proto->capacity_lineinfo;
        proto->capacity_lineinfo = XR_GROW_CAPACITY(old_capacity);
        proto->lineinfo = XR_GROW_ARRAY(int, proto->lineinfo,
                                        old_capacity, proto->capacity_lineinfo);
    }
    
    /* 记录行号 */
    proto->lineinfo[proto->size_lineinfo] = line;
    
    /* 增加计数 */
    proto->sizecode++;
    proto->size_lineinfo++;
}

/*
** 添加常量到常量池
** 返回常量索引
*/
int xr_bc_proto_add_constant(Proto *proto, XrValue value) {
    return xr_valuearray_add(&proto->constants, value);
}

/*
** 添加嵌套函数原型
** 返回原型索引
*/
int xr_bc_proto_add_proto(Proto *proto, Proto *child) {
    /* 扩容 */
    if (proto->sizeprotos >= proto->capacity_protos) {
        int old_capacity = proto->capacity_protos;
        proto->capacity_protos = XR_GROW_CAPACITY(old_capacity);
        proto->protos = XR_GROW_ARRAY(Proto*, proto->protos,
                                      old_capacity, proto->capacity_protos);
    }
    
    /* 添加 */
    proto->protos[proto->sizeprotos] = child;
    return proto->sizeprotos++;
}

/*
** 添加upvalue信息
** 返回upvalue索引
*/
int xr_bc_proto_add_upvalue(Proto *proto, uint8_t index, uint8_t is_local) {
    /* 检查是否已存在 */
    for (int i = 0; i < proto->sizeupvalues; i++) {
        UpvalInfo *uv = &proto->upvalues[i];
        if (uv->index == index && uv->is_local == is_local) {
            return i;  /* 已存在，返回索引 */
        }
    }
    
    /* 扩容 */
    if (proto->sizeupvalues >= proto->capacity_upvalues) {
        int old_capacity = proto->capacity_upvalues;
        proto->capacity_upvalues = XR_GROW_CAPACITY(old_capacity);
        proto->upvalues = XR_GROW_ARRAY(UpvalInfo, proto->upvalues,
                                        old_capacity, proto->capacity_upvalues);
    }
    
    /* 添加新upvalue */
    UpvalInfo *uv = &proto->upvalues[proto->sizeupvalues];
    uv->index = index;
    uv->is_local = is_local;
    
    return proto->sizeupvalues++;
}


/*
** xdebug.c
** Xray 字节码调试工具实现
*/

#include "xdebug.h"
#include "xstring.h"
#include <stdio.h>

/* ========== 辅助函数 ========== */

/*
** 简单指令（无操作数）
*/
static int simple_instruction(const char *name, int offset) {
    printf("%-16s\n", name);
    return offset + 1;
}

/*
** 单字节参数指令
*/
static int byte_instruction(const char *name, Proto *proto, int offset) {
    Instruction inst = proto->code[offset];
    uint8_t arg = GETARG_A(inst);
    printf("%-16s %4d\n", name, arg);
    return offset + 1;
}

/*
** 常量指令
*/
static int constant_instruction(const char *name, Proto *proto, int offset) {
    Instruction inst = proto->code[offset];
    uint8_t ra = GETARG_A(inst);
    uint16_t kbx = GETARG_Bx(inst);
    
    printf("%-16s R[%d] K[%d] ; ", name, ra, kbx);
    
    /* 打印常量值 */
    if (kbx < proto->constants.count) {
        xr_print_value(proto->constants.values[kbx]);
    } else {
        printf("???");
    }
    printf("\n");
    
    return offset + 1;
}

/*
** ABC格式指令
*/
static int abc_instruction(const char *name, Proto *proto, int offset) {
    Instruction inst = proto->code[offset];
    uint8_t ra = GETARG_A(inst);
    uint8_t rb = GETARG_B(inst);
    uint8_t rc = GETARG_C(inst);
    
    printf("%-16s R[%d] R[%d] R[%d]\n", name, ra, rb, rc);
    return offset + 1;
}

/*
** AB格式指令
*/
static int ab_instruction(const char *name, Proto *proto, int offset) {
    Instruction inst = proto->code[offset];
    uint8_t ra = GETARG_A(inst);
    uint8_t rb = GETARG_B(inst);
    
    printf("%-16s R[%d] R[%d]\n", name, ra, rb);
    return offset + 1;
}

/*
** 立即数指令（AsBx格式）
*/
static int immediate_instruction(const char *name, Proto *proto, int offset) {
    Instruction inst = proto->code[offset];
    uint8_t ra = GETARG_A(inst);
    int sbx = GETARG_sBx(inst);
    
    printf("%-16s R[%d] %d\n", name, ra, sbx);
    return offset + 1;
}

/*
** 跳转指令
*/
static int jump_instruction(const char *name, int sign, Proto *proto, int offset) {
    Instruction inst = proto->code[offset];
    int sj = GETARG_sJ(inst);
    
    printf("%-16s %d -> %d\n", name, sj, offset + 1 + sign * sj);
    return offset + 1;
}

/* ========== 值打印 ========== */

/*
** 打印常量值
*/
void xr_print_value(XrValue value) {
#if XR_NAN_TAGGING
    /* NaN Tagging模式（未实现） */
    if (XR_IS_NULL(value)) {
        printf("null");
    } else if (XR_IS_BOOL(value)) {
        printf(XR_TO_BOOL(value) ? "true" : "false");
    } else if (XR_IS_NUM(value)) {
        printf("%.14g", XR_TO_FLOAT(value));
    } else if (XR_IS_OBJ(value)) {
        printf("<object>");
    } else {
        printf("<?>");
    }
#else
    /* Tagged Union模式 */
    switch (value.type) {
        case XR_TNULL:
            printf("null");
            break;
        case XR_TBOOL:
            printf(value.as.b ? "true" : "false");
            break;
        case XR_TINT:
            printf("%ld", value.as.i);
            break;
        case XR_TFLOAT:
            printf("%.14g", value.as.n);
            break;
        case XR_TSTRING: {
            XrString *str = (XrString *)value.as.obj;
            if (str != NULL) {
                printf("\"%s\"", str->chars);
            } else {
                printf("null");
            }
            break;
        }
        case XR_TFUNCTION:
            printf("<function>");
            break;
        case XR_TARRAY:
            printf("<array>");
            break;
        case XR_TMAP:
            printf("<map>");
            break;
        case XR_TCLASS:
            printf("<class>");
            break;
        case XR_TINSTANCE:
            printf("<instance>");
            break;
        default:
            printf("<?>");
            break;
    }
#endif
}

/*
** 打印常量表
*/
void xr_print_constants(Proto *proto) {
    if (proto->constants.count == 0) {
        return;
    }
    
    printf("Constants:\n");
    for (int i = 0; i < proto->constants.count; i++) {
        printf("  K[%d] = ", i);
        xr_print_value(proto->constants.values[i]);
        printf("\n");
    }
}

/* ========== 反汇编API ========== */

/*
** 反汇编单条指令
*/
int xr_disassemble_instruction(Proto *proto, int offset) {
    printf("%04d ", offset);
    
    /* 显示行号 */
    if (offset > 0 && 
        proto->lineinfo != NULL &&
        offset < proto->size_lineinfo &&
        proto->lineinfo[offset] == proto->lineinfo[offset - 1]) {
        printf("   | ");
    } else if (proto->lineinfo != NULL && offset < proto->size_lineinfo) {
        printf("%4d ", proto->lineinfo[offset]);
    } else {
        printf("   ? ");
    }
    
    /* 获取指令 */
    Instruction inst = proto->code[offset];
    OpCode op = GET_OPCODE(inst);
    const char *name = xr_opcode_name(op);
    
    /* 根据操作码类型反汇编 */
    switch (op) {
        /* 简单指令 */
        case OP_NOP:
            return simple_instruction(name, offset);
        
        /* 加载指令 */
        case OP_LOADNIL:
        case OP_LOADTRUE:
        case OP_LOADFALSE:
            return byte_instruction(name, proto, offset);
        
        case OP_LOADI:
        case OP_LOADF:
            return immediate_instruction(name, proto, offset);
        
        case OP_LOADK:
        case OP_GETGLOBAL:
            return constant_instruction(name, proto, offset);
        
        /* 寄存器操作 */
        case OP_MOVE:
        case OP_UNM:
        case OP_NOT:
            return ab_instruction(name, proto, offset);
        
        /* 算术运算 */
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_MOD:
            return abc_instruction(name, proto, offset);
        
        case OP_ADDI:
        case OP_SUBI:
        case OP_MULI:
            return immediate_instruction(name, proto, offset);
        
        case OP_ADDK:
        case OP_SUBK:
        case OP_MULK:
        case OP_DIVK:
        case OP_MODK:
            return abc_instruction(name, proto, offset);
        
        /* 比较运算 */
        case OP_EQ:
        case OP_LT:
        case OP_LE:
        case OP_GT:
        case OP_GE:
            return abc_instruction(name, proto, offset);
        
        case OP_EQI:
        case OP_LTI:
        case OP_LEI:
        case OP_GTI:
        case OP_GEI:
            return immediate_instruction(name, proto, offset);
        
        case OP_EQK:
            return abc_instruction(name, proto, offset);
        
        /* 控制流 */
        case OP_JMP:
            return jump_instruction(name, 1, proto, offset);
        
        case OP_TEST:
        case OP_TESTSET:
            return ab_instruction(name, proto, offset);
        
        case OP_CALL:
        case OP_RETURN:
            return abc_instruction(name, proto, offset);
        
        /* 表操作 */
        case OP_NEWTABLE:
            return byte_instruction(name, proto, offset);
        
        case OP_GETTABLE:
        case OP_SETTABLE:
        case OP_GETI:
        case OP_SETI:
        case OP_GETFIELD:
        case OP_SETFIELD:
        case OP_SETLIST:
            return abc_instruction(name, proto, offset);
        
        /* 闭包 */
        case OP_CLOSURE:
            return constant_instruction(name, proto, offset);
        
        case OP_GETUPVAL:
        case OP_SETUPVAL:
            return ab_instruction(name, proto, offset);
        
        case OP_CLOSE:
            return byte_instruction(name, proto, offset);
        
        /* OOP */
        case OP_CLASS:
            return byte_instruction(name, proto, offset);
        
        case OP_INHERIT:
        case OP_GETPROP:
        case OP_SETPROP:
        case OP_GETSUPER:
        case OP_INVOKE:
        case OP_SUPERINVOKE:
        case OP_METHOD:
            return abc_instruction(name, proto, offset);
        
        /* 全局变量 */
        case OP_SETGLOBAL:
        case OP_DEFGLOBAL:
            return constant_instruction(name, proto, offset);
        
        default:
            printf("Unknown opcode %d\n", op);
            return offset + 1;
    }
}

/*
** 反汇编整个函数原型
*/
void xr_disassemble_proto(Proto *proto, const char *name) {
    printf("== ");
    if (name != NULL) {
        printf("%s ", name);
    } else if (proto->name != NULL) {
        printf("%s ", proto->name->chars);
    } else {
        printf("<script> ");
    }
    printf("==\n");
    
    /* 打印函数信息 */
    printf("Parameters: %d, Stack: %d, Code: %d\n",
           proto->numparams, proto->maxstacksize, proto->sizecode);
    
    /* 打印常量表 */
    if (proto->constants.count > 0) {
        xr_print_constants(proto);
        printf("\n");
    }
    
    /* 反汇编所有指令 */
    for (int offset = 0; offset < proto->sizecode;) {
        offset = xr_disassemble_instruction(proto, offset);
    }
    
    /* 反汇编嵌套函数 */
    if (proto->sizeprotos > 0) {
        printf("\nNested functions:\n");
        for (int i = 0; i < proto->sizeprotos; i++) {
            printf("\n");
            xr_disassemble_proto(proto->protos[i], NULL);
        }
    }
}


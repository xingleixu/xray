/*
** main.c
** Xray 解释器主程序
** 提供命令行接口
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xray.h"
#include "xparse.h"
#include "xcompiler.h"
#include "xvm.h"
#include "xdebug.h"

/* 打印版本信息 */
static void print_version(void) {
    printf("%s\n", XRAY_VERSION);
    printf("%s\n", XRAY_COPYRIGHT);
    printf("Value Mode: ");
#if XR_NAN_TAGGING
    printf("NaN Tagging (8 bytes)\n");
#else
    printf("Tagged Union (16 bytes)\n");
#endif
    printf("sizeof(XrValue) = %zu bytes\n", sizeof(XrValue));
}

/* 打印使用帮助 */
static void print_usage(const char *prog) {
    printf("用法: %s [选项] [脚本文件]\n", prog);
    printf("选项:\n");
    printf("  -v          显示版本信息\n");
    printf("  -h          显示此帮助信息\n");
    printf("  -e <代码>   执行字符串代码\n");
    printf("  --dump-ast  打印 AST 结构（调试用）\n");
    printf("  --dump-bc   打印字节码（调试用）\n");
}

/*
** 读取文件内容
*/
static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) return NULL;
    
    /* 获取文件大小 */
    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);
    
    /* 分配缓冲区 */
    char *buffer = (char *)malloc(file_size + 1);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }
    
    /* 读取文件 */
    size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
    if (bytes_read < file_size) {
        fprintf(stderr, "无法读取文件\n");
        free(buffer);
        fclose(file);
        return NULL;
    }
    
    buffer[bytes_read] = '\0';
    fclose(file);
    return buffer;
}

/*
** 执行 Xray 代码
** 默认使用字节码VM（高性能模式）
*/
static int run(XrayState *X, const char *source, int dump_ast, int dump_bc) {
    /* 1. 解析源代码为 AST */
    AstNode *ast = xr_parse(X, source);
    if (ast == NULL) {
        fprintf(stderr, "解析失败\n");
        return 1;
    }
    
    /* 2. 如果需要，打印 AST 结构 */
    if (dump_ast) {
        printf("=== AST 结构 ===\n");
        xr_ast_print(ast, 0);
        printf("=== 结束 ===\n\n");
    }
    
    /* 3. 编译到字节码 */
    Proto *proto = xr_compile(ast);
    if (proto == NULL) {
        fprintf(stderr, "编译失败\n");
        xr_ast_free(X, ast);
        return 1;
    }
    
    /* 4. 如果需要，打印字节码 */
    if (dump_bc) {
        printf("=== 字节码 ===\n");
        xr_disassemble_proto(proto, "main");
        printf("=== 结束 ===\n\n");
    }
    
    /* 5. 在字节码VM上执行 */
    xr_bc_vm_init();
    InterpretResult result = xr_bc_interpret_proto(proto);
    xr_bc_vm_free();
    
    int status = (result == INTERPRET_OK) ? 0 : 1;
    
    /* 6. 清理 */
    xr_bc_proto_free(proto);
    xr_ast_free(X, ast);
    
    return status;
}

/* REPL (Read-Eval-Print Loop) */
static int repl(XrayState *X) {
    (void)X;  /* 抑制未使用警告 */
    
    printf("%s REPL\n", XRAY_VERSION);
    fprintf(stderr, "错误: REPL模式暂未实现（需要字节码VM支持）\n");
    fprintf(stderr, "提示: 使用 './xray script.xr' 执行脚本文件\n");
    
    return 1;
}

/* 主函数 */
int main(int argc, char *argv[]) {
    XrayState *X;
    int status = 0;
    int dump_ast = 0;
    int dump_bc = 0;
    
    /* 创建 Xray 状态 */
    X = xray_newstate();
    if (X == NULL) {
        fprintf(stderr, "无法创建 Xray 状态\n");
        return 1;
    }
    
    /* 解析命令行参数 */
    if (argc == 1) {
        /* 没有参数，启动 REPL */
        status = repl(X);
    } else {
        int i;
        /* 先扫描选项 */
        for (i = 1; i < argc; i++) {
            if (argv[i][0] == '-' && argv[i][1] == '-') {
                if (strcmp(argv[i], "--dump-ast") == 0) {
                    dump_ast = 1;
                } else if (strcmp(argv[i], "--dump-bc") == 0) {
                    dump_bc = 1;
                }
            }
        }
        
        /* 再处理命令 */
        for (i = 1; i < argc; i++) {
            if (argv[i][0] == '-') {
                /* 选项 */
                if (argv[i][1] == '-') {
                    /* 长选项，已处理 */
                    continue;
                }
                
                switch (argv[i][1]) {
                    case 'v':
                        print_version();
                        break;
                    case 'h':
                        print_usage(argv[0]);
                        break;
                    case 'e':
                        if (i + 1 < argc) {
                            status = run(X, argv[++i], dump_ast, dump_bc);
                        } else {
                            fprintf(stderr, "错误: -e 需要一个参数\n");
                            status = 1;
                        }
                        break;
                    default:
                        fprintf(stderr, "未知选项: %s\n", argv[i]);
                        print_usage(argv[0]);
                        status = 1;
                        break;
                }
            } else {
                /* 文件名 */
                char *source = read_file(argv[i]);
                if (source != NULL) {
                    status = run(X, source, dump_ast, dump_bc);
                    free(source);
                } else {
                    fprintf(stderr, "无法读取文件: %s\n", argv[i]);
                    status = 1;
                }
                break;
            }
        }
    }
    
    /* 清理 */
    xray_close(X);
    
    return status;
}


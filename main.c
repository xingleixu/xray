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
#include "xeval.h"
#include "xscope.h"

/* 打印版本信息 */
static void print_version(void) {
    printf("%s\n", XRAY_VERSION);
    printf("%s\n", XRAY_COPYRIGHT);
}

/* 打印使用帮助 */
static void print_usage(const char *prog) {
    printf("用法: %s [选项] [脚本文件]\n", prog);
    printf("选项:\n");
    printf("  -v          显示版本信息\n");
    printf("  -h          显示此帮助信息\n");
    printf("  -e <代码>   执行字符串代码\n");
    printf("  --dump-ast  打印 AST 结构（调试用）\n");
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
*/
static int run(XrayState *X, const char *source, int dump_ast) {
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
    
    /* 3. 执行 AST */
    XrValue result = xr_eval(X, ast);
    
    /* 4. 释放 AST */
    xr_ast_free(X, ast);
    
    return 0;
}

/* REPL (Read-Eval-Print Loop) */
static int repl(XrayState *X) {
    char line[1024];
    
    printf("%s REPL\n", XRAY_VERSION);
    printf("输入表达式进行计算，输入 'exit' 退出\n");
    
    /* 创建持久的符号表，在 REPL 会话期间保持变量 */
    XSymbolTable *symbols = xsymboltable_new();
    if (!symbols) {
        fprintf(stderr, "无法创建符号表\n");
        return 1;
    }
    
    for (;;) {
        printf("xray> ");
        
        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n");
            break;
        }
        
        /* 移除换行符 */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        
        /* 跳过空行 */
        if (strlen(line) == 0) {
            continue;
        }
        
        /* 检查退出命令 */
        if (strcmp(line, "exit") == 0) {
            break;
        }
        
        /* 解析并执行代码 */
        AstNode *ast = xr_parse(X, line);
        if (ast != NULL) {
            /* 如果是表达式语句，显示结果 */
            if (ast->as.program.count > 0) {
                AstNode *stmt = ast->as.program.statements[0];
                if (stmt->type == AST_EXPR_STMT) {
                    /* 使用持久符号表求值 */
                    XrValue result = xr_eval_with_symbols(X, ast, symbols);
                    const char *result_str = xr_value_to_string(X, result);
                    printf("=> %s\n", result_str);
                } else {
                    /* 其他语句（如 print、变量声明），直接执行 */
                    xr_eval_with_symbols(X, ast, symbols);
                }
            }
            xr_ast_free(X, ast);
        }
    }
    
    /* 释放符号表 */
    xsymboltable_free(symbols);
    
    return 0;
}

/* 主函数 */
int main(int argc, char *argv[]) {
    XrayState *X;
    int status = 0;
    
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
        for (i = 1; i < argc; i++) {
            if (argv[i][0] == '-') {
                /* 选项 */
                switch (argv[i][1]) {
                    case 'v':
                        print_version();
                        break;
                    case 'h':
                        print_usage(argv[0]);
                        break;
                    case 'e':
                        if (i + 1 < argc) {
                            status = run(X, argv[++i], 0);
                        } else {
                            fprintf(stderr, "错误: -e 需要一个参数\n");
                            status = 1;
                        }
                        break;
                    case '-':
                        /* 长选项 */
                        if (strcmp(argv[i], "--dump-ast") == 0) {
                            if (i + 1 < argc) {
                                /* 下一个参数应该是文件名 */
                                i++;
                                char *source = read_file(argv[i]);
                                if (source != NULL) {
                                    status = run(X, source, 1);  /* 打印 AST */
                                    free(source);
                                } else {
                                    fprintf(stderr, "无法读取文件: %s\n", argv[i]);
                                    status = 1;
                                }
                            } else {
                                fprintf(stderr, "错误: --dump-ast 需要一个文件名\n");
                                status = 1;
                            }
                        } else {
                            fprintf(stderr, "未知选项: %s\n", argv[i]);
                            print_usage(argv[0]);
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
                    status = run(X, source, 0);
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


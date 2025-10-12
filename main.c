/*
** main.c
** Xray 解释器主程序
** 提供命令行接口
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xray.h"

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
}

/* REPL (Read-Eval-Print Loop) */
static int repl(XrayState *X) {
    char line[1024];
    
    printf("%s\n", XRAY_VERSION);
    printf("输入 'exit' 退出\n");
    
    for (;;) {
        printf("xray> ");
        
        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;
        }
        
        /* 移除换行符 */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        
        /* 检查退出命令 */
        if (strcmp(line, "exit") == 0) {
            break;
        }
        
        /* 执行代码 */
        xray_dostring(X, line);
    }
    
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
                            status = xray_dostring(X, argv[++i]);
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
                status = xray_dofile(X, argv[i]);
                break;
            }
        }
    }
    
    /* 清理 */
    xray_close(X);
    
    return status;
}


/*
** test_for_loop.c
** 测试 for 循环中的分号分隔符
*/

#include <stdio.h>
#include "../xlex.h"

int main(void) {
    printf("=== 测试 for 循环分号分隔符 ===\n\n");
    
    const char *source = "for (let i = 0; i < 10; i = i + 1) { }";
    
    Scanner scanner;
    xr_scanner_init(&scanner, source);
    
    printf("源代码: %s\n\n", source);
    printf("Token 序列:\n");
    
    Token token;
    int semicolon_count = 0;
    
    while ((token = xr_scanner_scan(&scanner)).type != TK_EOF) {
        printf("  [%s] '%.*s' (行 %d)\n", 
               xr_token_name(token.type),
               token.length,
               token.start,
               token.line);
        
        if (token.type == TK_SEMICOLON) {
            semicolon_count++;
        }
    }
    
    printf("\n检测到的分号数量: %d\n", semicolon_count);
    
    if (semicolon_count == 2) {
        printf("✓ for 循环中的两个分号分隔符识别正确!\n");
        return 0;
    } else {
        printf("✗ 错误: 应该识别出 2 个分号，但只识别出 %d 个\n", semicolon_count);
        return 1;
    }
}


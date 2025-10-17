/*
** xparse_oop.c
** Xray OOP语法解析实现
** 
** v0.12.0：第十二阶段 - Parser扩展
** 
** 功能：
**   - 解析class声明
**   - 解析new表达式
**   - 解析this/super表达式
**   - 解析成员访问和赋值
*/

#include "xparse.h"
#include "xast.h"
#include "xmem.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* 辅助函数：复制Token文本为字符串 */
static char* token_to_string(Token *token) {
    if (!token || token->length == 0) return NULL;
    
    char *str = (char*)xmem_alloc(token->length + 1);
    memcpy(str, token->start, token->length);
    str[token->length] = '\0';
    return str;
}

/* ========== 类声明解析 ========== */

/*
** 解析类声明
** 
** 语法：
**   class Dog extends Animal {
**       field_declarations...
**       method_declarations...
**   }
*/
AstNode *xr_parse_class_declaration(Parser *parser) {
    int line = parser->previous.line;
    
    /* 消费 'class' 关键字（已经匹配了）*/
    
    /* 解析类名 */
    consume(parser, TK_NAME, "期望类名");
    char *class_name = token_to_string(&parser->previous);
    
    /* 解析extends子句（可选）*/
    char *super_name = NULL;
    if (match(parser, TK_EXTENDS)) {
        consume(parser, TK_NAME, "期望超类名");
        super_name = token_to_string(&parser->previous);
    }
    
    /* 解析类体 */
    consume(parser, TK_LBRACE, "期望'{'开始类体");
    
    /* 收集字段和方法声明 */
    AstNode **fields = NULL;
    int field_count = 0;
    int field_capacity = 0;
    
    AstNode **methods = NULL;
    int method_count = 0;
    int method_capacity = 0;
    
    while (!check(parser, TK_RBRACE) && !check(parser, TK_EOF)) {
        /* 检查是否是方法还是字段 */
        bool is_method = false;
        AstNode *member = xr_parse_field_declaration(parser, &is_method);
        
        if (is_method) {
            /* 添加到方法数组 */
            if (method_count >= method_capacity) {
                method_capacity = method_capacity == 0 ? 4 : method_capacity * 2;
                methods = (AstNode**)xmem_realloc(methods, 
                                                   sizeof(AstNode*) * method_count,
                                                   sizeof(AstNode*) * method_capacity);
            }
            methods[method_count++] = member;
        } else {
            /* 添加到字段数组 */
            if (field_count >= field_capacity) {
                field_capacity = field_capacity == 0 ? 4 : field_capacity * 2;
                fields = (AstNode**)xmem_realloc(fields,
                                                  sizeof(AstNode*) * field_count,
                                                  sizeof(AstNode*) * field_capacity);
            }
            fields[field_count++] = member;
        }
    }
    
    consume(parser, TK_RBRACE, "期望'}'结束类体");
    
    /* 创建类声明AST节点 */
    return xr_ast_class_decl(parser->X, class_name, super_name,
                             fields, field_count,
                             methods, method_count, line);
}

/* ========== 字段声明解析 ========== */

/*
** 解析字段或方法声明
** 
** 语法：
**   字段：name: string 或 private age: int = 0
**   方法：greet() { ... } 或 constructor(name) { ... }
** 
** @param is_method_out 输出参数，true表示是方法，false表示是字段
*/
AstNode *xr_parse_field_declaration(Parser *parser, bool *is_method_out) {
    int line = parser->current.line;
    
    /* 解析访问修饰符（可选）*/
    bool is_private = false;
    bool is_static = false;
    bool is_getter = false;
    bool is_setter = false;
    
    if (match(parser, TK_PRIVATE)) {
        is_private = true;
    } else if (match(parser, TK_PUBLIC)) {
        is_private = false;  /* 显式public */
    }
    
    if (match(parser, TK_STATIC)) {
        is_static = true;
    }
    
    if (match(parser, TK_GET)) {
        is_getter = true;
    } else if (match(parser, TK_SET)) {
        is_setter = true;
    }
    
    /* v0.19.0：检查operator关键字 */
    if (match(parser, TK_OPERATOR)) {
        *is_method_out = true;
        return xr_parse_operator_method(parser, is_private, is_static);
    }
    
    /* 解析成员名（可能是constructor关键字）*/
    char *name = NULL;
    bool is_constructor = false;
    
    if (match(parser, TK_CONSTRUCTOR)) {
        /* constructor关键字 */
        is_constructor = true;
        name = (char*)xmem_alloc(12);
        strcpy(name, "constructor");
    } else {
        /* 普通名称 */
        consume(parser, TK_NAME, "期望字段或方法名");
        name = token_to_string(&parser->previous);
    }
    
    /* 区分字段和方法：看是否有'(' */
    if (check(parser, TK_LPAREN) || is_constructor) {
        /* 方法：有参数列表 */
        *is_method_out = true;
        return xr_parse_method_declaration(parser, name, is_private, is_static);
    } else {
        /* 字段：有类型注解或初始化 */
        *is_method_out = false;
        
        /* 解析类型注解（可选）*/
        char *type_name = NULL;
        if (match(parser, TK_COLON)) {
            /* 类型可以是类型关键字或类名 */
            if (match(parser, TK_TYPE_INT)) {
                type_name = (char*)xmem_alloc(4);
                strcpy(type_name, "int");
            } else if (match(parser, TK_TYPE_FLOAT)) {
                type_name = (char*)xmem_alloc(6);
                strcpy(type_name, "float");
            } else if (match(parser, TK_TYPE_STRING)) {
                type_name = (char*)xmem_alloc(7);
                strcpy(type_name, "string");
            } else if (match(parser, TK_BOOL)) {
                type_name = (char*)xmem_alloc(5);
                strcpy(type_name, "bool");
            } else if (match(parser, TK_NAME)) {
                type_name = token_to_string(&parser->previous);
            } else {
                error(parser, "期望类型名");
            }
        }
        
        /* 解析初始化表达式（可选）*/
        AstNode *initializer = NULL;
        if (match(parser, TK_ASSIGN)) {
            initializer = xr_parse_expression(parser);
        }
        
        /* 字段声明不需要分号（Xray不用分号）*/
        
        return xr_ast_field_decl(parser->X, name, type_name,
                                is_private, is_static,
                                initializer, line);
    }
}

/* ========== 方法声明解析 ========== */

/*
** 解析方法声明
** 
** 语法：
**   greet(name: string): void { ... }
**   constructor(x: int, y: int) { ... }
** 
** @param name 方法名（已解析）
** @param is_private 是否私有
** @param is_static 是否静态
*/
AstNode *xr_parse_method_declaration(Parser *parser, const char *name, 
                                     bool is_private, bool is_static) {
    int line = parser->previous.line;
    
    /* 检查是否是构造函数 */
    bool is_constructor = (strcmp(name, "constructor") == 0);
    
    /* 解析参数列表 */
    consume(parser, TK_LPAREN, "期望'('开始参数列表");
    
    char **parameters = NULL;
    char **param_types = NULL;
    int param_count = 0;
    int param_capacity = 0;
    
    if (!check(parser, TK_RPAREN)) {
        do {
            /* 扩展数组 */
            if (param_count >= param_capacity) {
                param_capacity = param_capacity == 0 ? 4 : param_capacity * 2;
                parameters = (char**)xmem_realloc(parameters,
                                                   sizeof(char*) * param_count,
                                                   sizeof(char*) * param_capacity);
                param_types = (char**)xmem_realloc(param_types,
                                                    sizeof(char*) * param_count,
                                                    sizeof(char*) * param_capacity);
            }
            
            /* 解析参数名 */
            consume(parser, TK_NAME, "期望参数名");
            parameters[param_count] = token_to_string(&parser->previous);
            
            /* 解析参数类型（可选）*/
            if (match(parser, TK_COLON)) {
                /* 类型可以是类型关键字或类名 */
                if (match(parser, TK_TYPE_INT)) {
                    param_types[param_count] = (char*)xmem_alloc(4);
                    strcpy(param_types[param_count], "int");
                } else if (match(parser, TK_TYPE_FLOAT)) {
                    param_types[param_count] = (char*)xmem_alloc(6);
                    strcpy(param_types[param_count], "float");
                } else if (match(parser, TK_TYPE_STRING)) {
                    param_types[param_count] = (char*)xmem_alloc(7);
                    strcpy(param_types[param_count], "string");
                } else if (match(parser, TK_BOOL)) {
                    param_types[param_count] = (char*)xmem_alloc(5);
                    strcpy(param_types[param_count], "bool");
                } else if (match(parser, TK_NAME)) {
                    param_types[param_count] = token_to_string(&parser->previous);
                } else {
                    error(parser, "期望类型名");
                    param_types[param_count] = NULL;
                }
            } else {
                param_types[param_count] = NULL;
            }
            
            param_count++;
        } while (match(parser, TK_COMMA));
    }
    
    consume(parser, TK_RPAREN, "期望')'结束参数列表");
    
    /* 解析返回类型（可选）*/
    char *return_type = NULL;
    if (match(parser, TK_COLON)) {
        /* 类型可以是类型关键字或类名 */
        if (match(parser, TK_TYPE_INT)) {
            return_type = (char*)xmem_alloc(4);
            strcpy(return_type, "int");
        } else if (match(parser, TK_TYPE_FLOAT)) {
            return_type = (char*)xmem_alloc(6);
            strcpy(return_type, "float");
        } else if (match(parser, TK_TYPE_STRING)) {
            return_type = (char*)xmem_alloc(7);
            strcpy(return_type, "string");
        } else if (match(parser, TK_BOOL)) {
            return_type = (char*)xmem_alloc(5);
            strcpy(return_type, "bool");
        } else if (match(parser, TK_VOID)) {
            return_type = (char*)xmem_alloc(5);
            strcpy(return_type, "void");
        } else if (match(parser, TK_NAME)) {
            return_type = token_to_string(&parser->previous);
        } else {
            error(parser, "期望返回类型名");
        }
    }
    
    /* 解析方法体 */
    consume(parser, TK_LBRACE, "期望'{'开始方法体");
    AstNode *body = xr_parse_block(parser);
    
    /* 创建方法声明节点（v0.12.1: 传递访问控制标志）*/
    return xr_ast_method_decl(parser->X, name,
                              parameters, param_types, param_count,
                              return_type, body,
                              is_constructor, is_static, is_private,
                              false, false, line);          /* is_getter, is_setter */
}

/* ========== new表达式解析 ========== */

/*
** 解析new表达式
** 
** 语法：new Dog("Rex", "Labrador")
*/
AstNode *xr_parse_new_expression(Parser *parser) {
    int line = parser->previous.line;
    
    /* 消费 'new' 关键字（已匹配）*/
    
    /* 解析类名 */
    consume(parser, TK_NAME, "期望类名");
    char *class_name = token_to_string(&parser->previous);
    
    /* 解析构造参数 */
    consume(parser, TK_LPAREN, "期望'('开始参数列表");
    
    AstNode **arguments = NULL;
    int arg_count = 0;
    int arg_capacity = 0;
    
    if (!check(parser, TK_RPAREN)) {
        do {
            /* 扩展数组 */
            if (arg_count >= arg_capacity) {
                arg_capacity = arg_capacity == 0 ? 4 : arg_capacity * 2;
                arguments = (AstNode**)xmem_realloc(arguments,
                                                     sizeof(AstNode*) * arg_count,
                                                     sizeof(AstNode*) * arg_capacity);
            }
            
            /* 解析参数表达式 */
            arguments[arg_count++] = xr_parse_expression(parser);
        } while (match(parser, TK_COMMA));
    }
    
    consume(parser, TK_RPAREN, "期望')'结束参数列表");
    
    /* 创建new表达式节点 */
    return xr_ast_new_expr(parser->X, class_name, arguments, arg_count, line);
}

/* ========== this表达式解析 ========== */

/*
** 解析this表达式
** 
** 语法：this
*/
AstNode *xr_parse_this_expression(Parser *parser) {
    int line = parser->previous.line;
    
    /* 消费 'this' 关键字（已匹配）*/
    
    /* 创建this表达式节点 */
    return xr_ast_this_expr(parser->X, line);
}

/* ========== super表达式解析 ========== */

/*
** 解析super调用
** 
** 语法：
**   super.greet(args)  - 调用超类方法
**   super(args)        - 调用超类构造函数
*/
AstNode *xr_parse_super_expression(Parser *parser) {
    int line = parser->previous.line;
    
    /* 消费 'super' 关键字（已匹配）*/
    
    char *method_name = NULL;
    
    /* 检查是super()还是super.method() */
    if (match(parser, TK_DOT)) {
        /* super.method() */
        consume(parser, TK_NAME, "期望方法名");
        method_name = token_to_string(&parser->previous);
        
        /* 解析参数列表 */
        consume(parser, TK_LPAREN, "期望'('开始参数列表");
    } else if (check(parser, TK_LPAREN)) {
        /* super(args) - 调用超类构造函数 */
        method_name = NULL;  /* NULL表示构造函数 */
    } else {
        error(parser, "期望'.'或'('在super之后");
        return NULL;
    }
    
    /* 解析参数 */
    AstNode **arguments = NULL;
    int arg_count = 0;
    int arg_capacity = 0;
    
    if (!check(parser, TK_RPAREN)) {
        do {
            if (arg_count >= arg_capacity) {
                arg_capacity = arg_capacity == 0 ? 4 : arg_capacity * 2;
                arguments = (AstNode**)xmem_realloc(arguments,
                                                     sizeof(AstNode*) * arg_count,
                                                     sizeof(AstNode*) * arg_capacity);
            }
            arguments[arg_count++] = xr_parse_expression(parser);
        } while (match(parser, TK_COMMA));
    }
    
    consume(parser, TK_RPAREN, "期望')'结束参数列表");
    
    /* 创建super调用节点 */
    return xr_ast_super_call(parser->X, method_name, arguments, arg_count, line);
}

/* ========== 运算符方法解析（v0.19.0新增）========== */

/*
** 解析运算符方法声明
** 
** 语法：
**   operator +(other: Type): Type { ... }
** 
** @param is_private 是否私有
** @param is_static 是否静态
** 
** 第一个版本：只支持二元 + 运算符
*/
AstNode *xr_parse_operator_method(Parser *parser, bool is_private, bool is_static) {
    int line = parser->previous.line;
    
    /* 解析运算符符号 */
    TokenType op_token = parser->current.type;
    
    /* 第一个版本只支持 + 运算符 */
    if (op_token != TK_PLUS) {
        error(parser, "当前版本只支持 operator +");
        return NULL;
    }
    
    xr_parser_advance(parser);  /* 消费运算符 token */
    
    /* 运算符方法名就是符号本身 */
    char *name = (char*)xmem_alloc(2);
    strcpy(name, "+");
    
    /* 解析参数列表 */
    consume(parser, TK_LPAREN, "期望'('开始参数列表");
    
    char **parameters = NULL;
    char **param_types = NULL;
    int param_count = 0;
    
    /* 二元运算符必须有且只有一个参数 */
    if (check(parser, TK_RPAREN)) {
        error(parser, "二元运算符 + 需要一个参数");
        return NULL;
    }
    
    /* 解析第一个参数 */
    consume(parser, TK_NAME, "期望参数名");
    
    parameters = (char**)xmem_alloc(sizeof(char*));
    param_types = (char**)xmem_alloc(sizeof(char*));
    parameters[0] = token_to_string(&parser->previous);
    
    /* 解析参数类型（可选）*/
    if (match(parser, TK_COLON)) {
        /* 跳过类型（简化版本）*/
        if (match(parser, TK_TYPE_INT) || match(parser, TK_TYPE_FLOAT) || 
            match(parser, TK_TYPE_STRING) || match(parser, TK_BOOL) ||
            match(parser, TK_NAME)) {
            param_types[0] = token_to_string(&parser->previous);
        } else {
            error(parser, "期望类型名");
            param_types[0] = NULL;
        }
    } else {
        param_types[0] = NULL;
    }
    
    param_count = 1;
    
    /* 不应该有更多参数 */
    if (match(parser, TK_COMMA)) {
        error(parser, "二元运算符 + 只能有一个参数");
        return NULL;
    }
    
    consume(parser, TK_RPAREN, "期望')'结束参数列表");
    
    /* 解析返回类型（可选）*/
    char *return_type = NULL;
    if (match(parser, TK_COLON)) {
        if (match(parser, TK_TYPE_INT)) {
            return_type = (char*)xmem_alloc(4);
            strcpy(return_type, "int");
        } else if (match(parser, TK_TYPE_FLOAT)) {
            return_type = (char*)xmem_alloc(6);
            strcpy(return_type, "float");
        } else if (match(parser, TK_TYPE_STRING)) {
            return_type = (char*)xmem_alloc(7);
            strcpy(return_type, "string");
        } else if (match(parser, TK_NAME)) {
            return_type = token_to_string(&parser->previous);
        } else {
            error(parser, "期望返回类型");
        }
    }
    
    /* 解析方法体 */
    consume(parser, TK_LBRACE, "期望'{'开始方法体");
    AstNode *body = xr_parse_block(parser);
    
    /* 创建方法节点 */
    AstNode *method = xr_ast_method_decl(parser->X, name, 
                                        parameters, param_types, param_count,
                                        return_type, body,
                                        false,  /* is_constructor */
                                        is_static,
                                        is_private,
                                        false,  /* is_getter */
                                        false,  /* is_setter */
                                        line);
    
    /* 设置运算符标记 */
    method->as.method_decl.is_operator = true;  /* 标记为运算符方法 */
    method->as.method_decl.op_type = OP_BINARY;  /* 二元运算符 */
    
    return method;
}


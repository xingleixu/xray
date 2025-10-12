# Xray 语言 Makefile
# 使用 ANSI C 标准编译

# 编译器和编译选项
CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -g
LDFLAGS = 

# 目标文件
TARGET = xray
TEST_TARGET = test/test_lexer
TEST_FOR_TARGET = test/test_for_loop
TEST_AST_TARGET = test/test_ast
TEST_PARSER_TARGET = test/test_parser
TEST_EVAL_TARGET = test/test_eval
TEST_VARIABLES_TARGET = test/test_variables
TEST_CONTROL_FLOW_TARGET = test/test_control_flow

# 源文件
CORE_SRCS = xvalue.c xlex.c xstate.c xast.c xparse.c xeval.c xscope.c
MAIN_SRC = main.c
TEST_SRC = test/test_lexer.c
TEST_FOR_SRC = test/test_for_loop.c
TEST_AST_SRC = test/test_ast.c
TEST_PARSER_SRC = test/test_parser.c
TEST_EVAL_SRC = test/test_eval.c
TEST_VARIABLES_SRC = test/test_variables.c
TEST_CONTROL_FLOW_SRC = test/test_control_flow.c

# 对象文件
CORE_OBJS = $(CORE_SRCS:.c=.o)
MAIN_OBJ = $(MAIN_SRC:.c=.o)
TEST_OBJ = $(TEST_SRC:.c=.o)
TEST_FOR_OBJ = $(TEST_FOR_SRC:.c=.o)
TEST_AST_OBJ = $(TEST_AST_SRC:.c=.o)
TEST_PARSER_OBJ = $(TEST_PARSER_SRC:.c=.o)
TEST_EVAL_OBJ = $(TEST_EVAL_SRC:.c=.o)
TEST_VARIABLES_OBJ = $(TEST_VARIABLES_SRC:.c=.o)
TEST_CONTROL_FLOW_OBJ = $(TEST_CONTROL_FLOW_SRC:.c=.o)

# 默认目标
all: $(TARGET)

# 编译主程序
$(TARGET): $(CORE_OBJS) $(MAIN_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "编译完成: $(TARGET)"

# 编译测试程序
test: $(TEST_TARGET) $(TEST_FOR_TARGET) $(TEST_AST_TARGET) $(TEST_PARSER_TARGET) $(TEST_EVAL_TARGET) $(TEST_VARIABLES_TARGET) $(TEST_CONTROL_FLOW_TARGET)
	@echo "运行词法分析器测试..."
	./$(TEST_TARGET)
	@echo ""
	@echo "运行 for 循环测试..."
	./$(TEST_FOR_TARGET)
	@echo ""
	@echo "运行 AST 测试..."
	./$(TEST_AST_TARGET)
	@echo ""
	@echo "运行语法解析器测试..."
	./$(TEST_PARSER_TARGET)
	@echo ""
	@echo "运行表达式求值器测试..."
	./$(TEST_EVAL_TARGET)
	@echo ""
	@echo "运行变量和作用域测试..."
	./$(TEST_VARIABLES_TARGET)
	@echo ""
	@echo "运行控制流测试..."
	./$(TEST_CONTROL_FLOW_TARGET)

$(TEST_TARGET): $(CORE_OBJS) $(TEST_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "编译完成: $(TEST_TARGET)"

$(TEST_FOR_TARGET): $(CORE_OBJS) $(TEST_FOR_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "编译完成: $(TEST_FOR_TARGET)"

$(TEST_AST_TARGET): $(CORE_OBJS) $(TEST_AST_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "编译完成: $(TEST_AST_TARGET)"

$(TEST_PARSER_TARGET): $(CORE_OBJS) $(TEST_PARSER_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "编译完成: $(TEST_PARSER_TARGET)"

$(TEST_EVAL_TARGET): $(CORE_OBJS) $(TEST_EVAL_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "编译完成: $(TEST_EVAL_TARGET)"

$(TEST_VARIABLES_TARGET): $(CORE_OBJS) $(TEST_VARIABLES_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "编译完成: $(TEST_VARIABLES_TARGET)"

$(TEST_CONTROL_FLOW_TARGET): $(CORE_OBJS) $(TEST_CONTROL_FLOW_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "编译完成: $(TEST_CONTROL_FLOW_TARGET)"

# 编译规则
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# 清理
clean:
	rm -f $(CORE_OBJS) $(MAIN_OBJ) $(TEST_OBJ) $(TEST_FOR_OBJ) $(TEST_AST_OBJ) $(TEST_PARSER_OBJ) $(TEST_EVAL_OBJ) $(TEST_VARIABLES_OBJ) $(TEST_CONTROL_FLOW_OBJ) $(TARGET) $(TEST_TARGET) $(TEST_FOR_TARGET) $(TEST_AST_TARGET) $(TEST_PARSER_TARGET) $(TEST_EVAL_TARGET) $(TEST_VARIABLES_TARGET) $(TEST_CONTROL_FLOW_TARGET)
	@echo "清理完成"

# 重新编译
rebuild: clean all

# 帮助信息
help:
	@echo "Xray 语言构建系统"
	@echo ""
	@echo "可用目标:"
	@echo "  all      - 编译主程序 (默认)"
	@echo "  test     - 编译并运行测试"
	@echo "  clean    - 清理生成的文件"
	@echo "  rebuild  - 清理后重新编译"
	@echo "  help     - 显示此帮助信息"

.PHONY: all test clean rebuild help


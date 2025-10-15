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
TEST_FUNCTIONS_TARGET = test/test_functions
TEST_VALUE_TYPE_TARGET = test/test_value_type
TEST_MEM_TARGET = test/test_mem
TEST_STRUCTURES_TARGET = test/test_structures
TEST_ARRAY_TARGET = test/test_array
TEST_STRING_TARGET = test/test_string
TEST_MAP_TARGET = test/test_map
TEST_CLASS_BASIC_TARGET = test/test_class_basic
TEST_CLASS_INHERIT_TARGET = test/test_class_inherit

# 源文件
CORE_SRCS = xvalue.c xtype.c xlex.c xstate.c xast.c xparse.c xparse_type.c xparse_oop.c xeval.c xeval_oop.c xscope.c xmem.c fn_proto.c upvalue.c closure.c xarray.c xstring.c xhash.c xmap.c xhashmap.c xclass.c xinstance.c xmethod.c
MAIN_SRC = main.c
TEST_SRC = test/test_lexer.c
TEST_FOR_SRC = test/test_for_loop.c
TEST_AST_SRC = test/test_ast.c
TEST_PARSER_SRC = test/test_parser.c
TEST_EVAL_SRC = test/test_eval.c
TEST_VARIABLES_SRC = test/test_variables.c
TEST_CONTROL_FLOW_SRC = test/test_control_flow.c
TEST_FUNCTIONS_SRC = test/test_functions.c
TEST_VALUE_TYPE_SRC = test/test_value_type.c
TEST_MEM_SRC = test/test_mem.c
TEST_STRUCTURES_SRC = test/test_structures.c
TEST_ARRAY_SRC = test/test_array.c
TEST_STRING_SRC = test/test_string.c
TEST_MAP_SRC = test/test_map.c
TEST_CLASS_BASIC_SRC = test/test_class_basic.c
TEST_CLASS_INHERIT_SRC = test/test_class_inherit.c

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
TEST_FUNCTIONS_OBJ = $(TEST_FUNCTIONS_SRC:.c=.o)
TEST_VALUE_TYPE_OBJ = $(TEST_VALUE_TYPE_SRC:.c=.o)
TEST_MEM_OBJ = $(TEST_MEM_SRC:.c=.o)
TEST_STRUCTURES_OBJ = $(TEST_STRUCTURES_SRC:.c=.o)
TEST_ARRAY_OBJ = $(TEST_ARRAY_SRC:.c=.o)
TEST_STRING_OBJ = $(TEST_STRING_SRC:.c=.o)
TEST_MAP_OBJ = $(TEST_MAP_SRC:.c=.o)
TEST_CLASS_BASIC_OBJ = $(TEST_CLASS_BASIC_SRC:.c=.o)
TEST_CLASS_INHERIT_OBJ = $(TEST_CLASS_INHERIT_SRC:.c=.o)

# 默认目标
all: $(TARGET)

# 编译主程序
$(TARGET): $(CORE_OBJS) $(MAIN_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "编译完成: $(TARGET)"

# 编译测试程序
test: $(TEST_TARGET) $(TEST_FOR_TARGET) $(TEST_AST_TARGET) $(TEST_PARSER_TARGET) $(TEST_EVAL_TARGET) $(TEST_VARIABLES_TARGET) $(TEST_CONTROL_FLOW_TARGET) $(TEST_FUNCTIONS_TARGET) $(TEST_VALUE_TYPE_TARGET) $(TEST_MEM_TARGET) $(TEST_STRUCTURES_TARGET) $(TEST_ARRAY_TARGET) $(TEST_STRING_TARGET) $(TEST_MAP_TARGET) $(TEST_CLASS_BASIC_TARGET) $(TEST_CLASS_INHERIT_TARGET)
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
	@echo ""
	@echo "运行函数系统测试..."
	./$(TEST_FUNCTIONS_TARGET)
	@echo ""
	@echo "运行值对象和类型系统测试..."
	./$(TEST_VALUE_TYPE_TARGET)
	@echo ""
	@echo "运行内存管理测试..."
	./$(TEST_MEM_TARGET)
	@echo ""
	@echo "运行闭包数据结构测试..."
	./$(TEST_STRUCTURES_TARGET)
	@echo ""
	@echo "运行数组测试..."
	./$(TEST_ARRAY_TARGET)
	@echo ""
	@echo "运行字符串测试..."
	./$(TEST_STRING_TARGET)
	@echo ""
	@echo "运行Map测试..."
	./$(TEST_MAP_TARGET)
	@echo ""
	@echo "运行OOP基础测试..."
	./$(TEST_CLASS_BASIC_TARGET)
	@echo ""
	@echo "运行OOP继承测试..."
	./$(TEST_CLASS_INHERIT_TARGET)

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

$(TEST_FUNCTIONS_TARGET): $(CORE_OBJS) $(TEST_FUNCTIONS_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "编译完成: $(TEST_FUNCTIONS_TARGET)"

$(TEST_VALUE_TYPE_TARGET): $(CORE_OBJS) $(TEST_VALUE_TYPE_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "编译完成: $(TEST_VALUE_TYPE_TARGET)"

$(TEST_MEM_TARGET): xmem.o $(TEST_MEM_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "编译完成: $(TEST_MEM_TARGET)"

$(TEST_STRUCTURES_TARGET): xmem.o fn_proto.o upvalue.o closure.o $(TEST_STRUCTURES_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "编译完成: $(TEST_STRUCTURES_TARGET)"

$(TEST_ARRAY_TARGET): $(CORE_OBJS) $(TEST_ARRAY_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "编译完成: $(TEST_ARRAY_TARGET)"

$(TEST_STRING_TARGET): $(CORE_OBJS) $(TEST_STRING_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "编译完成: $(TEST_STRING_TARGET)"

$(TEST_MAP_TARGET): $(CORE_OBJS) $(TEST_MAP_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "编译完成: $(TEST_MAP_TARGET)"

$(TEST_CLASS_BASIC_TARGET): $(CORE_OBJS) $(TEST_CLASS_BASIC_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "编译完成: $(TEST_CLASS_BASIC_TARGET)"

$(TEST_CLASS_INHERIT_TARGET): $(CORE_OBJS) $(TEST_CLASS_INHERIT_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "编译完成: $(TEST_CLASS_INHERIT_TARGET)"

# 编译规则
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# 清理
clean:
	rm -f $(CORE_OBJS) $(MAIN_OBJ) $(TEST_OBJ) $(TEST_FOR_OBJ) $(TEST_AST_OBJ) $(TEST_PARSER_OBJ) $(TEST_EVAL_OBJ) $(TEST_VARIABLES_OBJ) $(TEST_CONTROL_FLOW_OBJ) $(TEST_FUNCTIONS_OBJ) $(TEST_VALUE_TYPE_OBJ) $(TEST_MEM_OBJ) $(TEST_STRUCTURES_OBJ) $(TEST_ARRAY_OBJ) $(TEST_STRING_OBJ) $(TEST_MAP_OBJ) $(TEST_CLASS_BASIC_OBJ) $(TEST_CLASS_INHERIT_OBJ) $(TARGET) $(TEST_TARGET) $(TEST_FOR_TARGET) $(TEST_AST_TARGET) $(TEST_PARSER_TARGET) $(TEST_EVAL_TARGET) $(TEST_VARIABLES_TARGET) $(TEST_CONTROL_FLOW_TARGET) $(TEST_FUNCTIONS_TARGET) $(TEST_VALUE_TYPE_TARGET) $(TEST_MEM_TARGET) $(TEST_STRUCTURES_TARGET) $(TEST_ARRAY_TARGET) $(TEST_STRING_TARGET) $(TEST_MAP_TARGET) $(TEST_CLASS_BASIC_TARGET) $(TEST_CLASS_INHERIT_TARGET)
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


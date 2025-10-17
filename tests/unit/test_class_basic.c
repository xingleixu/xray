/*
** test_class_basic.c
** Xray OOP系统基础测试
** 
** v0.12.0：第十二阶段 - Day 1-2 核心数据结构测试
*/

#include "xray.h"
#include "xstate.h"
#include "xclass.h"
#include "xinstance.h"
#include "xmethod.h"
#include "xvalue.h"
#include "xtype.h"
#include "xstring.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

/* 测试计数器 */
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        printf("Running test: %s ... ", #name); \
        tests_run++; \
        name(); \
        tests_passed++; \
        printf("PASSED\n"); \
    } while(0)

/* ========== Day 1-2: 核心数据结构测试 ========== */

/*
** 测试1：类创建
*/
void test_class_creation() {
    XrayState *X = xr_state_new();
    
    /* 创建空类 */
    XrClass *cls = xr_class_new(X, "TestClass", NULL);
    assert(cls != NULL);
    assert(strcmp(cls->name, "TestClass") == 0);
    assert(cls->super == NULL);
    assert(cls->field_count == 0);
    assert(cls->own_field_count == 0);
    assert(cls->methods != NULL);
    
    xr_class_free(cls);
    xr_state_free(X);
}

/*
** 测试2：类创建（带超类）
*/
void test_class_with_super() {
    XrayState *X = xr_state_new();
    
    /* 创建父类 */
    XrClass *super_cls = xr_class_new(X, "SuperClass", NULL);
    assert(super_cls != NULL);
    
    /* 创建子类 */
    XrClass *sub_cls = xr_class_new(X, "SubClass", super_cls);
    assert(sub_cls != NULL);
    assert(strcmp(sub_cls->name, "SubClass") == 0);
    assert(sub_cls->super == super_cls);
    
    xr_class_free(sub_cls);
    xr_class_free(super_cls);
    xr_state_free(X);
}

/*
** 测试3：实例创建
*/
void test_instance_creation() {
    XrayState *X = xr_state_new();
    
    XrClass *cls = xr_class_new(X, "Person", NULL);
    XrInstance *inst = xr_instance_new(X, cls);
    
    assert(inst != NULL);
    assert(inst->klass == cls);
    
    xr_instance_free(inst);
    xr_class_free(cls);
    xr_state_free(X);
}

/*
** 测试4：添加字段
*/
void test_add_field() {
    XrayState *X = xr_state_new();
    
    XrClass *cls = xr_class_new(X, "Person", NULL);
    
    /* 添加字段 */
    xr_class_add_field(cls, "name", xr_type_string(X));
    xr_class_add_field(cls, "age", xr_type_int(X));
    
    assert(cls->field_count == 2);
    assert(cls->own_field_count == 2);
    assert(strcmp(cls->field_names[0], "name") == 0);
    assert(strcmp(cls->field_names[1], "age") == 0);
    
    xr_class_free(cls);
    xr_state_free(X);
}

/*
** 测试5：查找字段索引
*/
void test_find_field_index() {
    XrayState *X = xr_state_new();
    
    XrClass *cls = xr_class_new(X, "Person", NULL);
    xr_class_add_field(cls, "name", xr_type_string(X));
    xr_class_add_field(cls, "age", xr_type_int(X));
    
    /* 查找存在的字段 */
    int index_name = xr_class_find_field_index(cls, "name");
    int index_age = xr_class_find_field_index(cls, "age");
    assert(index_name == 0);
    assert(index_age == 1);
    
    /* 查找不存在的字段 */
    int index_missing = xr_class_find_field_index(cls, "missing");
    assert(index_missing == -1);
    
    xr_class_free(cls);
    xr_state_free(X);
}

/*
** 测试6：实例字段访问
*/
void test_instance_field_access() {
    XrayState *X = xr_state_new();
    
    /* 创建类并添加字段 */
    XrClass *cls = xr_class_new(X, "Person", NULL);
    xr_class_add_field(cls, "name", xr_type_string(X));
    xr_class_add_field(cls, "age", xr_type_int(X));
    
    /* 创建实例 */
    XrInstance *inst = xr_instance_new(X, cls);
    
    /* 设置字段值 */
    XrString *name_str = xr_string_new("Alice", 5);
    xr_instance_set_field(inst, "name", xr_string_value(name_str));
    xr_instance_set_field(inst, "age", xr_int(30));
    
    /* 读取字段值 */
    XrValue name = xr_instance_get_field(inst, "name");
    XrValue age = xr_instance_get_field(inst, "age");
    
    assert(xr_isstring(name));
    assert(xr_isint(age));
    assert(xr_toint(age) == 30);
    assert(strcmp(xr_tostring(name)->chars, "Alice") == 0);
    
    xr_instance_free(inst);
    xr_class_free(cls);
    xr_state_free(X);
}

/*
** 测试7：通过索引访问字段（快速路径）
*/
void test_instance_field_by_index() {
    XrayState *X = xr_state_new();
    
    XrClass *cls = xr_class_new(X, "Point", NULL);
    xr_class_add_field(cls, "x", xr_type_int(X));
    xr_class_add_field(cls, "y", xr_type_int(X));
    
    XrInstance *inst = xr_instance_new(X, cls);
    
    /* 通过索引设置字段 */
    xr_instance_set_field_by_index(inst, 0, xr_int(10));
    xr_instance_set_field_by_index(inst, 1, xr_int(20));
    
    /* 通过索引读取字段 */
    XrValue x = xr_instance_get_field_by_index(inst, 0);
    XrValue y = xr_instance_get_field_by_index(inst, 1);
    
    assert(xr_toint(x) == 10);
    assert(xr_toint(y) == 20);
    
    xr_instance_free(inst);
    xr_class_free(cls);
    xr_state_free(X);
}

/*
** 测试8：方法对象创建
*/
void test_method_creation() {
    XrayState *X = xr_state_new();
    
    /* 创建一个简单函数（暂时用NULL，实际需要从AST创建）*/
    /* TODO: 完整测试需要函数对象支持 */
    XrFunction *func = xr_function_new("greet", NULL, NULL, 0, NULL, NULL);
    
    /* 创建方法 */
    XrMethod *method = xr_method_new(X, "greet", func, false);
    assert(method != NULL);
    assert(strcmp(method->name, "greet") == 0);
    assert(method->func == func);
    assert(method->is_static == false);
    assert(method->is_private == false);
    assert(method->is_constructor == false);
    
    xr_method_free(method);
    xr_function_free(func);
    xr_state_free(X);
}

/*
** 测试9：添加方法到类
*/
void test_add_method_to_class() {
    XrayState *X = xr_state_new();
    
    XrClass *cls = xr_class_new(X, "MyClass", NULL);
    XrFunction *func = xr_function_new("myMethod", NULL, NULL, 0, NULL, NULL);
    XrMethod *method = xr_method_new(X, "myMethod", func, false);
    
    /* 添加方法 */
    xr_class_add_method(cls, method);
    
    /* 查找方法 */
    XrMethod *found = xr_class_lookup_method(cls, "myMethod");
    assert(found == method);
    
    /* 查找不存在的方法 */
    XrMethod *not_found = xr_class_lookup_method(cls, "notExists");
    assert(not_found == NULL);
    
    xr_class_free(cls);
    xr_function_free(func);
    xr_method_free(method);
    xr_state_free(X);
}

/*
** 测试10：值系统集成
*/
void test_value_integration() {
    XrayState *X = xr_state_new();
    
    /* 创建类 */
    XrClass *cls = xr_class_new(X, "TestClass", NULL);
    XrValue cls_val = xr_value_from_class(cls);
    
    /* 类型检查 */
    assert(xr_is_class(cls_val));
    assert(!xr_is_instance(cls_val));
    
    /* 值转换 */
    XrClass *cls2 = xr_to_class(cls_val);
    assert(cls2 == cls);
    
    /* 创建实例 */
    XrInstance *inst = xr_instance_new(X, cls);
    XrValue inst_val = xr_value_from_instance(inst);
    
    /* 类型检查 */
    assert(xr_is_instance(inst_val));
    assert(!xr_is_class(inst_val));
    
    /* 值转换 */
    XrInstance *inst2 = xr_to_instance(inst_val);
    assert(inst2 == inst);
    
    xr_instance_free(inst);
    xr_class_free(cls);
    xr_state_free(X);
}

/*
** 测试11：is_a 检查（实例类型检查）
*/
void test_instance_is_a() {
    XrayState *X = xr_state_new();
    
    /* 创建类层次 */
    XrClass *animal = xr_class_new(X, "Animal", NULL);
    XrClass *dog = xr_class_new(X, "Dog", animal);
    XrClass *cat = xr_class_new(X, "Cat", animal);
    
    /* 创建实例 */
    XrInstance *dog_inst = xr_instance_new(X, dog);
    
    /* is_a 检查 */
    assert(xr_instance_is_a(dog_inst, dog) == true);      /* Dog is_a Dog */
    assert(xr_instance_is_a(dog_inst, animal) == true);   /* Dog is_a Animal */
    assert(xr_instance_is_a(dog_inst, cat) == false);     /* Dog is_not_a Cat */
    
    xr_instance_free(dog_inst);
    xr_class_free(cat);
    xr_class_free(dog);
    xr_class_free(animal);
    xr_state_free(X);
}

/*
** 测试12：类信息打印（调试功能）
*/
void test_class_print() {
    XrayState *X = xr_state_new();
    
    XrClass *cls = xr_class_new(X, "Person", NULL);
    xr_class_add_field(cls, "name", xr_type_string(X));
    xr_class_add_field(cls, "age", xr_type_int(X));
    
    printf("\n=== Class Print Test ===\n");
    xr_class_print(cls);
    printf("========================\n\n");
    
    xr_class_free(cls);
    xr_state_free(X);
}

/* ========== 主测试函数 ========== */

int main() {
    printf("\n");
    printf("========================================\n");
    printf(" Xray OOP 基础测试 (Day 1-2)\n");
    printf("========================================\n\n");
    
    /* Day 1-2: 核心数据结构测试 */
    TEST(test_class_creation);
    TEST(test_class_with_super);
    TEST(test_instance_creation);
    TEST(test_add_field);
    TEST(test_find_field_index);
    TEST(test_instance_field_access);
    TEST(test_instance_field_by_index);
    TEST(test_method_creation);
    TEST(test_add_method_to_class);
    TEST(test_value_integration);
    TEST(test_instance_is_a);
    TEST(test_class_print);
    
    /* 测试总结 */
    printf("\n");
    printf("========================================\n");
    printf(" 测试总结\n");
    printf("========================================\n");
    printf("运行测试: %d\n", tests_run);
    printf("通过测试: %d\n", tests_passed);
    printf("失败测试: %d\n", tests_run - tests_passed);
    
    if (tests_passed == tests_run) {
        printf("\n✅ 所有测试通过！\n\n");
        return 0;
    } else {
        printf("\n❌ 有测试失败！\n\n");
        return 1;
    }
}


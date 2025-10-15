/*
** test_class_inherit.c
** Xray OOP系统继承测试
** 
** v0.12.0：第十二阶段 - Day 5-9 方法调用与继承测试
*/

#include "../xray.h"
#include "../xstate.h"
#include "../xclass.h"
#include "../xinstance.h"
#include "../xmethod.h"
#include "../xvalue.h"
#include "../xtype.h"
#include "../xstring.h"
#include "../xast.h"
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

/* ========== Day 5-7: 方法调用测试 ========== */

/*
** 测试1：创建简单方法
*/
void test_simple_method() {
    XrayState *X = xr_state_new();
    
    /* 创建一个简单函数对象（无参数，无函数体）*/
    /* 暂时不测试实际调用，只测试数据结构 */
    XrFunction *func = xr_function_new("getValue", NULL, NULL, 0, NULL, NULL);
    
    /* 创建方法对象 */
    XrMethod *method = xr_method_new(X, "getValue", func, false);
    assert(method != NULL);
    assert(strcmp(method->name, "getValue") == 0);
    assert(!method->is_static);
    assert(!method->is_private);
    assert(!method->is_constructor);
    
    /* 清理 */
    xr_method_free(method);
    xr_function_free(func);
    xr_state_free(X);
}

/*
** 测试2：方法查找（单类）
*/
void test_method_lookup_single() {
    XrayState *X = xr_state_new();
    
    /* 创建类 */
    XrClass *cls = xr_class_new(X, "MyClass", NULL);
    
    /* 创建方法 */
    XrFunction *func1 = xr_function_new("method1", NULL, NULL, 0, NULL, NULL);
    XrFunction *func2 = xr_function_new("method2", NULL, NULL, 0, NULL, NULL);
    XrMethod *method1 = xr_method_new(X, "method1", func1, false);
    XrMethod *method2 = xr_method_new(X, "method2", func2, false);
    
    /* 添加方法 */
    xr_class_add_method(cls, method1);
    xr_class_add_method(cls, method2);
    
    /* 查找方法 */
    XrMethod *found1 = xr_class_lookup_method(cls, "method1");
    XrMethod *found2 = xr_class_lookup_method(cls, "method2");
    XrMethod *not_found = xr_class_lookup_method(cls, "notExists");
    
    assert(found1 == method1);
    assert(found2 == method2);
    assert(not_found == NULL);
    
    /* 清理 */
    xr_class_free(cls);
    xr_function_free(func1);
    xr_function_free(func2);
    xr_method_free(method1);
    xr_method_free(method2);
    xr_state_free(X);
}

/* ========== Day 8-9: 继承测试 ========== */

/*
** 测试3：单继承 - 字段继承
*/
void test_field_inheritance() {
    XrayState *X = xr_state_new();
    
    /* 创建父类 Animal {name} */
    XrClass *animal = xr_class_new(X, "Animal", NULL);
    xr_class_add_field(animal, "name", xr_type_string(X));
    
    assert(animal->field_count == 1);
    assert(animal->own_field_count == 1);
    
    /* 创建子类 Dog {breed} */
    XrClass *dog = xr_class_new(X, "Dog", NULL);
    xr_class_add_field(dog, "breed", xr_type_string(X));
    
    /* 设置继承关系 */
    xr_class_set_super(dog, animal);
    
    /* 验证字段继承 */
    assert(dog->field_count == 2);          /* name + breed */
    assert(dog->own_field_count == 1);      /* 只有breed是自己的 */
    assert(strcmp(dog->field_names[0], "name") == 0);
    assert(strcmp(dog->field_names[1], "breed") == 0);
    assert(dog->super == animal);
    
    /* 清理 */
    xr_class_free(dog);
    xr_class_free(animal);
    xr_state_free(X);
}

/*
** 测试4：多层继承 - Animal -> Dog -> Husky
*/
void test_multi_level_inheritance() {
    XrayState *X = xr_state_new();
    
    /* Animal {name} */
    XrClass *animal = xr_class_new(X, "Animal", NULL);
    xr_class_add_field(animal, "name", xr_type_string(X));
    
    /* Dog {breed} extends Animal */
    XrClass *dog = xr_class_new(X, "Dog", NULL);
    xr_class_add_field(dog, "breed", xr_type_string(X));
    xr_class_set_super(dog, animal);
    
    /* Husky {color} extends Dog */
    XrClass *husky = xr_class_new(X, "Husky", NULL);
    xr_class_add_field(husky, "color", xr_type_string(X));
    xr_class_set_super(husky, dog);
    
    /* 验证字段继承 */
    assert(husky->field_count == 3);        /* name + breed + color */
    assert(husky->own_field_count == 1);    /* 只有color */
    assert(strcmp(husky->field_names[0], "name") == 0);
    assert(strcmp(husky->field_names[1], "breed") == 0);
    assert(strcmp(husky->field_names[2], "color") == 0);
    
    /* 清理 */
    xr_class_free(husky);
    xr_class_free(dog);
    xr_class_free(animal);
    xr_state_free(X);
}

/*
** 测试5：方法查找（继承链）
*/
void test_method_lookup_inheritance() {
    XrayState *X = xr_state_new();
    
    /* 创建父类和方法 */
    XrClass *animal = xr_class_new(X, "Animal", NULL);
    XrFunction *move_func = xr_function_new("move", NULL, NULL, 0, NULL, NULL);
    XrMethod *move_method = xr_method_new(X, "move", move_func, false);
    xr_class_add_method(animal, move_method);
    
    /* 创建子类 */
    XrClass *dog = xr_class_new(X, "Dog", animal);
    XrFunction *bark_func = xr_function_new("bark", NULL, NULL, 0, NULL, NULL);
    XrMethod *bark_method = xr_method_new(X, "bark", bark_func, false);
    xr_class_add_method(dog, bark_method);
    
    /* 子类应该能找到父类的方法 */
    XrMethod *found_move = xr_class_lookup_method(dog, "move");
    XrMethod *found_bark = xr_class_lookup_method(dog, "bark");
    
    assert(found_move == move_method);  /* 从父类继承 */
    assert(found_bark == bark_method);  /* 自己的方法 */
    
    /* 父类不能找到子类的方法 */
    XrMethod *not_found = xr_class_lookup_method(animal, "bark");
    assert(not_found == NULL);
    
    /* 清理 */
    xr_class_free(dog);
    xr_class_free(animal);
    xr_function_free(move_func);
    xr_function_free(bark_func);
    xr_method_free(move_method);
    xr_method_free(bark_method);
    xr_state_free(X);
}

/*
** 测试6：is_a 继承检查
*/
void test_is_a_inheritance() {
    XrayState *X = xr_state_new();
    
    /* Animal -> Dog -> Husky */
    XrClass *animal = xr_class_new(X, "Animal", NULL);
    XrClass *dog = xr_class_new(X, "Dog", animal);
    XrClass *cat = xr_class_new(X, "Cat", animal);
    XrClass *husky = xr_class_new(X, "Husky", dog);
    
    /* 创建Husky实例 */
    XrInstance *husky_inst = xr_instance_new(X, husky);
    
    /* is_a 检查 */
    assert(xr_instance_is_a(husky_inst, husky) == true);   /* Husky is_a Husky */
    assert(xr_instance_is_a(husky_inst, dog) == true);     /* Husky is_a Dog */
    assert(xr_instance_is_a(husky_inst, animal) == true);  /* Husky is_a Animal */
    assert(xr_instance_is_a(husky_inst, cat) == false);    /* Husky is_not_a Cat */
    
    /* 清理 */
    xr_instance_free(husky_inst);
    xr_class_free(husky);
    xr_class_free(cat);
    xr_class_free(dog);
    xr_class_free(animal);
    xr_state_free(X);
}

/*
** 测试7：实例字段继承
*/
void test_instance_inherited_fields() {
    XrayState *X = xr_state_new();
    
    /* Animal {name} */
    XrClass *animal = xr_class_new(X, "Animal", NULL);
    xr_class_add_field(animal, "name", xr_type_string(X));
    
    /* Dog {breed} extends Animal */
    XrClass *dog = xr_class_new(X, "Dog", NULL);
    xr_class_add_field(dog, "breed", xr_type_string(X));
    xr_class_set_super(dog, animal);
    
    /* 创建Dog实例 */
    XrInstance *dog_inst = xr_instance_new(X, dog);
    
    /* 设置字段（包括继承的字段）*/
    XrString *name_str = xr_string_new("Rex", 3);
    XrString *breed_str = xr_string_new("Labrador", 8);
    
    xr_instance_set_field(dog_inst, "name", xr_string_value(name_str));    /* 继承字段 */
    xr_instance_set_field(dog_inst, "breed", xr_string_value(breed_str));  /* 自己的字段 */
    
    /* 读取字段 */
    XrValue name = xr_instance_get_field(dog_inst, "name");
    XrValue breed = xr_instance_get_field(dog_inst, "breed");
    
    assert(xr_isstring(name));
    assert(xr_isstring(breed));
    assert(strcmp(xr_tostring(name)->chars, "Rex") == 0);
    assert(strcmp(xr_tostring(breed)->chars, "Labrador") == 0);
    
    /* 清理 */
    xr_instance_free(dog_inst);
    xr_class_free(dog);
    xr_class_free(animal);
    xr_state_free(X);
}

/* ========== 主测试函数 ========== */

int main() {
    printf("\n");
    printf("========================================\n");
    printf(" Xray OOP 继承测试 (Day 5-9)\n");
    printf("========================================\n\n");
    
    /* Day 5-7: 方法管理测试 */
    TEST(test_simple_method);
    TEST(test_method_lookup_single);
    
    /* Day 8-9: 继承测试 */
    TEST(test_field_inheritance);
    TEST(test_multi_level_inheritance);
    TEST(test_method_lookup_inheritance);
    TEST(test_is_a_inheritance);
    TEST(test_instance_inherited_fields);
    
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


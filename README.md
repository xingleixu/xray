# Xray 脚本语言

一门轻量化、高性能、兼容性好的脚本语言，使用 ANSI C 开发。

## 项目简介

Xray 是一门参考 Lua、Wren、QuickJS 设计的脚本语言，语法类似 TypeScript。项目目标：

- **代码规模**：< 30,000 行
- **性能目标**：介于 Wren 和 Lua 之间
- **可维护性**：优先于极致性能
- **易学习**：代码清晰，注释丰富

## 当前状态

**版本**：v0.9.0  
**开发阶段**：第九阶段完成（数组）

### 已完成功能

#### 核心语言特性
- ✅ 完整的词法分析器（v0.1.0）
- ✅ 表达式求值器（v0.2.0）
  - 算术运算：`+`, `-`, `*`, `/`, `%`
  - 比较运算：`==`, `!=`, `<`, `<=`, `>`, `>=`
  - 逻辑运算：`&&`, `||`, `!`
  - 短路求值
- ✅ 变量与作用域（v0.3.0）
  - `let` 变量声明
  - `const` 常量声明
  - 变量赋值
  - 块级作用域
  - 变量遮蔽
- ✅ 控制流（v0.4.0）
  - `if-else` 条件语句
  - `while` 循环
  - `for` 循环
  - `break` 和 `continue`
- ✅ 函数系统（v0.5.0）
  - 函数定义和调用
  - 参数传递
  - 返回值
  - 递归函数
  - 函数作为一等公民
- ✅ 类型系统（v0.6.0）
  - 类型注解
  - 值携带类型信息
  - 运行时类型检查
- ✅ 类型推导（v0.7.0）
  - 自动类型推导
  - 泛型基础
  - 类型别名
- ✅ 闭包与箭头函数（v0.8.0）
  - 全局/块作用域闭包
  - 箭头函数语法 `(x) => x * 2`
  - 高阶函数
  - 函数式编程支持
- ✅ 数组（v0.9.0）
  - 数组字面量 `[1, 2, 3]`
  - 索引访问和赋值 `arr[0] = 10`
  - 基础方法: push/pop/unshift/shift/indexOf/contains
  - 高阶方法: forEach/map/filter/reduce
  - 与箭头函数完美结合

### 测试状态
- **C 语言测试**：185+ 通过 ✅
  - 词法分析器：53 个测试
  - AST 模块：10 个测试
  - 语法解析器：20+ 个测试
  - 表达式求值器：15+ 个测试
  - 值对象类型：13 个测试
  - 内存管理：10 个测试
  - 闭包数据结构：10 个测试
  - 数组：65 个测试
- **示例程序**：18+ 运行成功 ✅
- **编译状态**：无错误 ✅
- **内存泄漏**：0 ✅

## 快速开始

### 编译

```bash
cd xray
make
```

### 运行

```bash
# 显示版本
./xray -v

# 显示帮助
./xray -h

# 运行 REPL（交互式环境）
./xray

# 执行脚本文件
./xray example/07_variables.xr

# 执行代码字符串
./xray -e "let x = 10"

# 查看 AST 结构（调试）
./xray --dump-ast example/07_variables.xr
```

### 运行测试

```bash
make test
```

### 清理

```bash
make clean
```

## 语法示例

**重要**：Xray **不使用**分号作为语句结束符，语句通过换行自动结束。分号**仅用于** for 循环中的条件分割。

### 变量和常量
```javascript
// 变量声明
let x = 10
let name = "Xray"

// 变量赋值
x = 20

// 常量声明（不可修改）
const PI = 3.14

// 表达式
let sum = x + 10
print(sum)
```

### 作用域
```javascript
let x = "global"
{
    let x = "local"  // 遮蔽外层
    print(x)         // local
}
print(x)             // global
```

### 表达式
```javascript
// 算术运算
print(1 + 2 * 3)     // 7

// 比较运算
print(5 > 3)         // true

// 逻辑运算
print(true && false) // false
```

### 控制流
```javascript
// if-else
if (x > 5) {
    print("large")
} else {
    print("small")
}

// while 循环
while (i < 10) {
    print(i)
    i = i + 1
}

// for 循环
// 注意：分号用作条件分隔符
for (let i = 0; i < 10; i = i + 1) {
//              ^分隔  ^分隔
    print(i)
}

// break 和 continue
for (let i = 0; i < 10; i = i + 1) {
    if (i == 5) continue
    if (i == 8) break
    print(i)
}
```

### 函数
```javascript
// 函数定义
function add(a, b) {
    return a + b
}

// 函数调用
print(add(10, 20))  // 30

// 递归函数
function fib(n) {
    if (n <= 1) return n
    return fib(n - 1) + fib(n - 2)
}

print(fib(10))  // 55

// 函数作为一等公民
function apply(f, x) {
    return f(x)
}

print(apply((x) => x * 2, 5))  // 10
```

### 类型注解
```javascript
// 变量类型注解
let age: int = 25
let name: string = "Xray"

// 函数类型注解
function add(a: int, b: int): int {
    return a + b
}
```

### 箭头函数 (v0.8.0 新增)
```javascript
// 无参数
let pi = () => 3.14159

// 单参数
let square = (x) => x * x

// 多参数
let add = (a, b) => a + b

// 块体
let abs = (x) => {
    if (x < 0) return -x
    else return x
}

// 与闭包结合
let multiplier = 3
let triple = (x) => x * multiplier

print(square(5))   // 25
print(add(10, 20)) // 30
print(triple(7))   // 21
```

### 闭包 (v0.8.0 新增)
```javascript
// 全局变量闭包
let counter = 0

function increment() {
    counter = counter + 1
    return counter
}

print(increment())  // 1
print(increment())  // 2

// 块作用域闭包
{
    let x = 10
    function addX(y) {
        return x + y
    }
    print(addX(5))  // 15
}
```

### 高阶函数 (v0.8.0 新增)
```javascript
// 函数作为参数
function map(size, fn) {
    let i = 0
    while (i < size) {
        print(fn(i))
        i = i + 1
    }
}

map(5, (x) => x * 2)
// 输出: 0 2 4 6 8
```

### 数组 (v0.9.0 新增)
```javascript
// 数组字面量
let numbers = [1, 2, 3, 4, 5]
let nested = [[1, 2], [3, 4]]

// 索引访问
print(numbers[0])      // 1
numbers[1] = 99        // 索引赋值

// 基础方法
numbers.push(6)              // 添加元素
let last = numbers.pop()     // 移除最后元素
print(numbers.length)        // 数组长度

// 高阶方法（与箭头函数结合）
let doubled = numbers.map((x) => x * 2)
// [2, 4, 6, 8, 10]

let evens = numbers.filter((x) => x % 2 == 0)
// [2, 4]

let sum = numbers.reduce((acc, x) => acc + x, 0)
// 15

// 链式调用
let result = numbers
    .filter((x) => x % 2 == 0)
    .map((x) => x * x)
    .reduce((acc, x) => acc + x, 0)
// 220
```

### 数据类型
- `null` - 空值
- `int` - 整数
- `float` - 浮点数
- `string` - 字符串
- `array` - 数组 ✅
- `set` - 集合（计划中）
- `map` - 映射（计划中）
- `class` - 类（计划中）

## 项目结构

```
xray/
├── 核心模块
│   ├── xray.h              # 主头文件
│   ├── xvalue.h/c          # 值类型系统
│   ├── xtype.h/c           # 类型系统
│   ├── xstate.h/c          # 状态管理
│   ├── xlex.h/c            # 词法分析器
│   ├── xast.h/c            # 抽象语法树
│   ├── xparse.h/c          # 语法解析器（Pratt）
│   ├── xparse_type.c       # 类型解析
│   ├── xeval.h/c           # 表达式求值器
│   └── xscope.h/c          # 符号表和作用域
│
├── 内存管理 (v0.8.0)
│   ├── xmem.h/c            # 内存追踪
│   ├── xgc.h               # GC接口预留
│   └── xobject.h           # 统一对象分配
│
├── 闭包系统 (v0.8.0)
│   ├── fn_proto.h/c        # 函数原型
│   ├── upvalue.h/c         # Upvalue对象
│   └── closure.h/c         # 闭包对象
│
├── 数组系统 (v0.9.0新增)
│   └── xarray.h/c          # 动态数组实现
│
├── main.c              # 主程序
├── Makefile            # 构建文件
│
├── example/            # 示例程序 (18+个)
│   ├── 04_expressions.xr
│   ├── 15_functions.xr
│   ├── 20_type_basics.xr
│   ├── 22_type_inference.xr
│   ├── 26_closure_*.xr     # 闭包示例
│   ├── 27_arrow_functions.xr  # 箭头函数
│   ├── 28_advanced_functions.xr
│   ├── 29_array_basics.xr      # 数组基础
│   ├── 30_array_methods.xr     # 数组方法
│   ├── 31_array_higher_order.xr # 高阶方法
│   └── 32_array_advanced.xr     # 综合示例
│
└── test/               # 测试程序 (185+测试)
    ├── test_lexer.c
    ├── test_parser.c
    ├── test_eval.c
    ├── test_functions.c
    ├── test_value_type.c
    ├── test_mem.c          # 内存管理测试
    └── test_structures.c   # 闭包测试
```

## 开发路线图

- [x] **第1阶段**：词法分析器 ✅
- [x] **第2阶段**：表达式求值器 ✅
- [x] **第3阶段**：变量与作用域 ✅
- [x] **第4阶段**：控制流（if/while/for）✅
- [x] **第5阶段**：函数系统 ✅
- [x] **第6阶段**：类型系统 ✅
- [x] **第7阶段**：类型推导 ✅
- [x] **第8阶段**：闭包与箭头函数 ✅
- [x] **第9阶段**：数组 ✅ (当前)
- [ ] **第10阶段**：字符串增强 - 下一步
- [ ] **第11阶段**：Map（字典）
- [ ] **第12阶段**：面向对象（类）
- [ ] **第13阶段**：字节码虚拟机 + 完整闭包
- [ ] **第14阶段**：垃圾回收
- [ ] **第15阶段**：LLVM后端
- [ ] **第16+阶段**：标准库和优化

**当前进度**: 9/19 (47%)


## 参考资料

本项目在开发过程中参考了：
- **Lua** - 轻量高效的设计理念
- **Wren** - 优雅的语法和实现
- **QuickJS** - 完整的 JavaScript 实现

## 贡献指南

1. 代码必须清晰易懂，面向初学者
2. 所有注释使用中文
3. 每次提交前运行测试确保通过
4. 遵循项目的代码规范

## 许可证

待定

## 作者

Xray 开发团队

## 性能指标

- **代码规模**：~12,510 行
- **编译时间**：< 3 秒
- **测试覆盖**：185+ 个测试，100% 通过
- **可执行文件**：~85KB
- **内存泄漏**：0

---

**最后更新**：2025-10-14  
**当前版本**：v0.9.0


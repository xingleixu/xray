# Xray 构建指南

## 🚀 快速开始

### 方式1：使用CMake（推荐）⭐

```bash
# 1. 配置
cd xray
mkdir build && cd build
cmake ..

# 2. 编译
cmake --build . -j4

# 3. 运行
./xray -v

# 4. 测试
ctest
```

### 方式2：使用Makefile（传统）

```bash
cd xray
make clean
make
./xray -v
make test
```

---

## 🔧 CMake详细用法

### 基础命令

```bash
# 配置项目
cmake ..

# 编译（并行）
cmake --build . -j4

# 运行测试
ctest --output-on-failure

# 清理
make clean
```

### 编译选项

```bash
# Debug构建
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Release构建
cmake -DCMAKE_BUILD_TYPE=Release ..

# 禁用测试
cmake -DBUILD_TESTS=OFF ..

# 启用代码覆盖率
cmake -DENABLE_COVERAGE=ON ..
make
make coverage
```

### 运行测试

```bash
# 运行所有测试
ctest

# 运行特定测试
ctest -R test_xerror

# 详细输出
ctest --verbose

# 显示失败详情
ctest --output-on-failure
```

---

## 🛠️ 工具集成

### 代码格式化
```bash
make format  # 使用clang-format
```

### 静态分析
```bash
make analyze  # 使用cppcheck
```

### 代码覆盖率
```bash
cmake -DENABLE_COVERAGE=ON ..
make
make test
make coverage
# 查看 coverage_html/index.html
```

---

## 🎯 常用场景

### 日常开发
```bash
cd build
vim ../xvalue.c          # 修改代码
cmake --build . -j4      # 快速编译
./test_array             # 运行测试
```

### 完整验证
```bash
rm -rf build
mkdir build && cd build
cmake ..
cmake --build . -j4
ctest --output-on-failure
```

### 性能测试
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j4
./xray ../benchmark/fib.xr
```

---

## 📊 对比

| 特性 | Makefile | CMake |
|------|----------|-------|
| 依赖管理 | 手动 | 自动 ✅ |
| 测试发现 | 手动 | 自动 ✅ |
| 并行编译 | make -j | 默认优化 ✅ |
| 跨平台 | 部分 | 全平台 ✅ |
| IDE集成 | 有限 | 完美 ✅ |
| 编译速度 | 3.2s | 2.5s ⚡ |

---

## ❓ 故障排除

### CMake未找到
```bash
brew install cmake  # macOS
sudo apt install cmake  # Ubuntu
```

### 编译错误
```bash
rm -rf build
mkdir build && cd build
cmake .. --verbose
cmake --build . --verbose
```

### 测试失败
```bash
ctest --output-on-failure --verbose
./test_name  # 直接运行查看详情
```

---

## 📚 更多信息

- 详细文档：`docs/CMAKE_MIGRATION.md`
- CMake配置：`CMakeLists.txt`
- 传统构建：`Makefile`（保留）

---

**推荐使用 CMake 以获得最佳开发体验！** 🚀


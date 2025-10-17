# Xray 语言 CMake 构建配置
# v0.14.0: 重构后的新目录结构
# v0.15.0: 添加编译器优化（常量折叠）

cmake_minimum_required(VERSION 3.10)
project(xray VERSION 0.15.0 LANGUAGES C)

# ========== 编译选项 ==========

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -O2 -g)

# 特性开关
option(XR_NAN_TAGGING "Enable NaN Tagging optimization" ON)
option(XR_USE_GC "Use Garbage Collector" OFF)
option(BUILD_TESTS "Build test programs" ON)
option(ENABLE_COVERAGE "Enable code coverage" OFF)

if(ENABLE_COVERAGE)
    add_compile_options(--coverage)
    add_link_options(--coverage)
endif()

# ========== 包含目录 ==========

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/value
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/memory
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/error
    ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/lexer
    ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/parser
    ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/ast
    ${CMAKE_CURRENT_SOURCE_DIR}/src/backend/compiler
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compiler
    ${CMAKE_CURRENT_SOURCE_DIR}/src/backend/vm
    ${CMAKE_CURRENT_SOURCE_DIR}/src/backend/bytecode
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/closure
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/objects
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
)

# ========== 核心库源文件 ==========

# 核心层
file(GLOB CORE_ERROR_SRC "src/core/error/*.c")
file(GLOB CORE_MEMORY_SRC "src/core/memory/*.c")
file(GLOB CORE_VALUE_SRC "src/core/value/*.c")

# 前端层
file(GLOB FRONTEND_LEXER_SRC "src/frontend/lexer/*.c")
file(GLOB FRONTEND_PARSER_SRC "src/frontend/parser/*.c")
file(GLOB FRONTEND_AST_SRC "src/frontend/ast/*.c")

# 后端层
file(GLOB BACKEND_COMPILER_SRC "src/backend/compiler/*.c")
file(GLOB COMPILER_SRC "src/compiler/*.c")
file(GLOB BACKEND_VM_SRC "src/backend/vm/*.c")
file(GLOB BACKEND_BYTECODE_SRC "src/backend/bytecode/*.c")

# 运行时层
file(GLOB RUNTIME_CLOSURE_SRC "src/runtime/closure/*.c")
file(GLOB RUNTIME_OBJECTS_SRC "src/runtime/objects/*.c")

# 工具层
file(GLOB UTILS_SRC "src/utils/*.c")

# 合并所有源文件
set(CORE_SOURCES
    ${CORE_ERROR_SRC}
    ${CORE_MEMORY_SRC}
    ${CORE_VALUE_SRC}
    ${FRONTEND_LEXER_SRC}
    ${FRONTEND_PARSER_SRC}
    ${FRONTEND_AST_SRC}
    ${BACKEND_COMPILER_SRC}
    ${COMPILER_SRC}
    ${BACKEND_VM_SRC}
    ${BACKEND_BYTECODE_SRC}
    ${RUNTIME_CLOSURE_SRC}
    ${RUNTIME_OBJECTS_SRC}
    ${UTILS_SRC}
)

# 创建核心静态库
add_library(xray_core STATIC ${CORE_SOURCES})

target_include_directories(xray_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ========== 主程序 ==========

add_executable(xray main.c)
target_link_libraries(xray xray_core)

# ========== 测试程序 ==========

if(BUILD_TESTS)
    enable_testing()
    
    # 包含测试框架目录
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/tests/framework)
    
    # 自动添加单元测试
    file(GLOB TEST_SOURCES "tests/unit/*.c")
    foreach(test_src ${TEST_SOURCES})
        get_filename_component(test_name ${test_src} NAME_WE)
        add_executable(${test_name} ${test_src})
        target_link_libraries(${test_name} xray_core)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
    
endif()

# ========== 信息输出 ==========

message(STATUS "==================================")
message(STATUS "Xray Language Build (Refactored)")
message(STATUS "==================================")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Architecture: Layered")
message(STATUS "  Core:     src/core/")
message(STATUS "  Frontend: src/frontend/")
message(STATUS "  Backend:  src/backend/")
message(STATUS "  Runtime:  src/runtime/")
message(STATUS "  Utils:    src/utils/")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "NaN Tagging: ${XR_NAN_TAGGING}")
message(STATUS "Build Tests: ${BUILD_TESTS}")
message(STATUS "==================================")

# ========== 静态分析工具 ==========

find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    file(GLOB_RECURSE ALL_SOURCE_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${ALL_SOURCE_FILES}
        COMMENT "Formatting code..."
    )
endif()

find_program(CPPCHECK cppcheck)
if(CPPCHECK)
    add_custom_target(analyze
        COMMAND ${CPPCHECK} --enable=all --std=c99
                ${CMAKE_CURRENT_SOURCE_DIR}/src
        COMMENT "Running static analysis..."
    )
endif()


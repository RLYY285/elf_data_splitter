#!/bin/bash

set -e

PROJECT_NAME="elf-data-splitter"
GITHUB_USER="RLYY285"

echo "========================================="
echo "ELF Data Splitter - Project Setup"
echo "========================================="

# 第一步：创建项目目录结构
echo ""
echo "[1/5] Creating directory structure..."
mkdir -p $PROJECT_NAME/{src,include,tests,build,.vscode}

# 第二步：复制所有源文件
echo "[2/5] Copying source files..."

# CMakeLists.txt
cat > $PROJECT_NAME/CMakeLists.txt << 'CMAKEEOFMARK'
cmake_minimum_required(VERSION 3.10)
project(elf-data-splitter VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

set(SOURCES
    src/main.cpp
    src/elf_parser.cpp
    src/elf_splitter.cpp
    src/segment_handler.cpp
    src/offset_calculator.cpp
    src/common.cpp
)

set(HEADERS
    include/elf_splitter.h
    src/elf_parser.h
    src/segment_handler.h
    src/offset_calculator.h
    src/common.h
)

add_executable(elf-splitter ${SOURCES} ${HEADERS})

enable_testing()
add_executable(test_splitter tests/test_splitter.cpp ${SOURCES})
target_compile_definitions(test_splitter PRIVATE TESTING_MODE)
add_test(NAME SplitterTest COMMAND test_splitter)

install(TARGETS elf-splitter DESTINATION bin)
install(FILES include/elf_splitter.h DESTINATION include)
CMAKEEOFMARK

# .gitignore
cat > $PROJECT_NAME/.gitignore << 'GITIGNOREMARK'
# Build directories
build/
cmake_build/
out/
*.o
*.obj
*.a
*.lib
*.dll
*.so
*.dylib

# Executables
elf-splitter
elf-splitter.exe
test_splitter
test_splitter.exe

# CMake
CMakeFiles/
CMakeCache.txt
cmake_install.cmake
Makefile
*.cmake

# IDEs
.vscode/
.idea/
*.swp
*.swo
*.user
.DS_Store
GITIGNOREMARK

# README.md
cat > $PROJECT_NAME/README.md << 'READMEMARK'
# ELF Data Splitter

基于 UPX 处理思路的 ELF 文件数据切割和插入工具。

## 功能特性

- ✅ 按 "每隔 a 字节插入 b 字节" 的模式处理 ELF 数据
- ✅ 支持填充 0 或 NOP 指令
- ✅ 区分 ET_EXEC 和 ET_DYN 文件类型
- ✅ 自动约束处理防止段越界
- ✅ 支持 32-bit 和 64-bit ELF
- ✅ 支持多种架构（x86, x86_64, ARM, ARM64, MIPS）

## 编译

```bash
mkdir build
cd build
cmake ..
make
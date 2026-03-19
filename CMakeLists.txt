cmake_minimum_required(VERSION 3.10)
project(elf-data-splitter VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译器选项
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# 源文件
set(SOURCES
    src/main.cpp
    src/elf_parser.cpp
    src/elf_splitter.cpp
    src/segment_handler.cpp
    src/offset_calculator.cpp
)

set(HEADERS
    include/elf_splitter.h
    src/elf_parser.h
    src/segment_handler.h
    src/offset_calculator.h
    src/common.h
)

# 主程序
add_executable(elf-splitter ${SOURCES} ${HEADERS})

# 单元测试
enable_testing()
add_executable(test_splitter tests/test_splitter.cpp ${SOURCES})
target_compile_definitions(test_splitter PRIVATE TESTING_MODE)
add_test(NAME SplitterTest COMMAND test_splitter)

# 安装
install(TARGETS elf-splitter DESTINATION bin)
install(FILES include/elf_splitter.h DESTINATION include)
cmake_minimum_required(VERSION 3.10)
project(elf-data-splitter VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    add_compile_options(-Wno-class-memaccess)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# ========== 主程序源文件 ==========
set(SOURCES
    src/main.cpp
    src/common.cpp
    src/elf_parser.cpp
    src/elf_splitter.cpp
    src/segment_handler.cpp
    src/offset_calculator.cpp
    src/stub_generator.cpp
    src/stub_injector.cpp
)

set(HEADERS
    include/elf_splitter.h
    src/elf_parser.h
    src/segment_handler.h
    src/offset_calculator.h
    src/stub_generator.h
    src/stub_injector.h
    src/common.h
)

# ========== 主程序 ==========
add_executable(elf-splitter ${SOURCES} ${HEADERS})

# ========== 测试程序 ==========
set(TEST_SOURCES
    src/common.cpp
    src/elf_parser.cpp
    src/elf_splitter.cpp
    src/segment_handler.cpp
    src/offset_calculator.cpp
    src/stub_generator.cpp
    src/stub_injector.cpp
)

enable_testing()
add_executable(test_splitter tests/test_splitter.cpp ${TEST_SOURCES})
add_test(NAME SplitterTest COMMAND test_splitter)

install(TARGETS elf-splitter DESTINATION bin)
install(FILES include/elf_splitter.h DESTINATION include)
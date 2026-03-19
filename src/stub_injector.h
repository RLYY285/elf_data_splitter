#ifndef __STUB_INJECTOR_H__
#define __STUB_INJECTOR_H__

#include "common.h"
#include "elf_parser.h"
#include "stub_generator.h"
#include <vector>
#include <cstdint>

// Stub 注入选项
struct StubInjectionOptions {
    bool inject_stub = false;
    StubType stub_type = StubType::RESTORE_DATA;
    uint64_t stub_base_offset = 0;   // Stub 在文件中的基地址
    uint64_t stub_base_vaddr = 0;    // Stub 入口虚拟地址（可选）
    uint64_t stub_region_size = 0;   // 允许写入的区域大小（可选）
    bool has_fixed_stub_base = false;
    bool update_entry_point = false;    // 是否更新入口点到 stub
    bool preserve_original_entry = true; // 是否保留原始入口点
};

// Stub 注入器
class StubInjector {
public:
    StubInjector(const ElfParser& parser, Architecture arch);
    ~StubInjector();
    
    // 注入 stub 到文件中
    bool inject_restore_stub(
        std::vector<uint8_t>& file_data,
        const std::vector<uint64_t>& insert_vaddrs,
        const std::vector<uint64_t>& insert_sizes,
        const std::vector<uint64_t>& bytes_to_move,
        const StubInjectionOptions& options
    );
    
    // 注入自定义 stub
    bool inject_custom_stub(
        std::vector<uint8_t>& file_data,
        const std::vector<uint8_t>& stub_code,
        const StubInjectionOptions& options
    );
    
    // 注入加密 stub
    bool inject_decrypt_stub(
        std::vector<uint8_t>& file_data,
        uint64_t code_start,
        uint64_t code_size,
        const uint8_t* key,
        size_t key_size,
        const StubInjectionOptions& options
    );
    
    // 获取注入后的入口点
    uint64_t get_new_entry_point() const { return new_entry_point; }
    
    // 获取 stub 注入的位置
    uint64_t get_stub_offset() const { return stub_offset; }
    
    // 获取 stub 大小
    uint64_t get_stub_size() const { return stub_size; }
    
private:
    const ElfParser& parser;
    StubGenerator stub_gen;
    uint64_t new_entry_point;
    uint64_t stub_offset;
    uint64_t stub_size;
    
    // 内部函数
    bool find_stub_injection_point(
        uint64_t& injection_point,
        uint64_t required_size
    );
    
    bool update_program_headers(
        std::vector<uint8_t>& file_data,
        uint64_t stub_offset,
        uint64_t stub_size,
        uint64_t new_entry
    );
    
    bool update_elf_header(
        std::vector<uint8_t>& file_data,
        uint64_t new_entry
    );
    
    bool create_stub_segment(
        std::vector<uint8_t>& file_data,
        uint64_t stub_offset,
        uint64_t stub_size
    );
};

#endif // __STUB_INJECTOR_H__

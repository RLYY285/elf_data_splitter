#ifndef __ELF_PARSER_H__
#define __ELF_PARSER_H__

#include "common.h"
#include <vector>
#include <cstdint>
#include <elf.h>

// 程序段信息
struct SegmentInfo {
    uint32_t p_type;      // 段类型 (PT_LOAD, PT_DYNAMIC, etc.)
    uint64_t p_offset;    // 文件中的偏移
    uint64_t p_vaddr;     // 虚拟地址
    uint64_t p_paddr;     // 物理地址
    uint64_t p_filesz;    // 文件中的大小
    uint64_t p_memsz;     // 内存中的大小
    uint32_t p_flags;     // 权限���志 (PF_R, PF_W, PF_X)
    uint64_t p_align;     // 对齐要求
    
    // 计算段的结束位置（文件中）
    uint64_t get_end_offset() const { return p_offset + p_filesz; }
    
    // 检查是否为 LOAD 段
    bool is_load() const { return p_type == PT_LOAD; }
    
    // 检查是否为可执行
    bool is_executable() const { return p_flags & PF_X; }
    
    // 检查是否为可写
    bool is_writable() const { return p_flags & PF_W; }
    
    // 检查是否为可读
    bool is_readable() const { return p_flags & PF_R; }
};

// Section 信息
struct SectionInfo {
    std::string name;
    uint32_t sh_type;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint64_t sh_addr;
    
    uint64_t get_end_offset() const { return sh_offset + sh_size; }
};

// ELF 文件解析器
class ElfParser {
public:
    ElfParser();
    ~ElfParser();
    
    // 加载 ELF 文件
    bool load(const std::string& filename);
    
    // 获取 ELF 信息
    const std::vector<uint8_t>& get_file_data() const { return file_data; }
    const std::vector<SegmentInfo>& get_segments() const { return segments; }
    const std::vector<SectionInfo>& get_sections() const { return sections; }
    
    // ELF 头信息
    bool is_64bit() const { return ei_class == ELFCLASS64; }
    bool is_little_endian() const { return ei_data == ELFDATA2LSB; }
    uint16_t get_machine() const { return e_machine; }
    uint16_t get_type() const { return e_type; }
    uint32_t get_phnum() const { return e_phnum; }
    uint32_t get_shnum() const { return e_shnum; }
    uint64_t get_entry() const { return e_entry; }
    uint64_t get_phoff() const { return e_phoff; }
    
    // 检查是否为可执行文件 (ET_EXEC)
    bool is_executable() const { return e_type == ET_EXEC; }
    
    // 检查是否为共享库 (ET_DYN)
    bool is_shared_object() const { return e_type == ET_DYN; }
    
    // 根据偏移查找对应的段
    int find_segment_by_offset(uint64_t offset) const;
    
    // 根据偏移查找对应的 section
    int find_section_by_offset(uint64_t offset) const;
    
    // 获取错误信息
    const std::string& get_last_error() const { return last_error; }
    
private:
    std::vector<uint8_t> file_data;
    std::vector<SegmentInfo> segments;
    std::vector<SectionInfo> sections;
    
    // ELF 头信息
    uint8_t ei_class;   // ELFCLASS32 or ELFCLASS64
    uint8_t ei_data;    // ELFDATA2LSB or ELFDATA2MSB
    uint16_t e_machine; // EM_386, EM_X86_64, etc.
    uint16_t e_type;    // ET_EXEC, ET_DYN, etc.
    uint32_t e_phnum;   // 程序头数量
    uint32_t e_shnum;   // section 头数量
    uint64_t e_entry;   // 入口点
    uint64_t e_phoff;   // 程序头偏移
    uint64_t e_shoff;   // section 头偏移
    uint32_t e_shstrndx; // section 名称字符串表索引
    
    std::string last_error;
    
    // 内部解析函数
    bool parse_elf_header();
    bool parse_program_headers();
    bool parse_section_headers();
    bool parse_section_names();
};

#endif // __ELF_PARSER_H__
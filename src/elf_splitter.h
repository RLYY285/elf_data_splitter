#ifndef __ELF_SPLITTER_H__
#define __ELF_SPLITTER_H__

#include "elf_parser.h"
#include "segment_handler.h"
#include "offset_calculator.h"
#include "stub_generator.h"
#include "stub_injector.h"
#include "common.h"
#include <vector>
#include <string>

// ========== Stub 注入配置 ==========
struct ElfSplitterOptions {
    // 数据处理
    size_t interval = 0;
    size_t insert_size = 0;
    bool use_nop = false;
    Architecture arch = Architecture::UNKNOWN;
    bool auto_optimize = true;
    bool allow_exec_segment_edit = true;
    
    // Stub 注入
    bool inject_stub = false;
    StubType stub_type = StubType::RESTORE_DATA;
    bool update_entry_point = false;
    std::vector<uint8_t> custom_stub_code;  // 自定义 stub
};

// ELF 分割和 Stub 注入处理器
class ElfSplitter {
public:
    ElfSplitter();
    ~ElfSplitter();
    
    // 设置处理选项
    void set_options(const ElfSplitterOptions& options);
    void set_options(
        size_t interval,
        size_t insert_size,
        bool use_nop,
        Architecture arch,
        bool auto_optimize);
    
    // 加载 ELF 文件
    bool load_elf(const std::string& filename);
    
    // 处理 ELF 文件（包括 Stub 注入）
    bool process();
    
    // 保存处理后的 ELF 文件
    bool save_elf(const std::string& output_filename);
    
    // 分析文件
    bool analyze();
    
    // 获取错误信息
    const std::string& get_last_error() const { return last_error; }
    
    // 获取统计信息
    struct Statistics {
        size_t total_segments_processed;
        size_t total_bytes_inserted;
        size_t segments_skipped;
        
        // Stub 信息
        bool stub_injected;
        uint64_t stub_offset;
        uint64_t stub_size;
        uint64_t new_entry_point;
        
        std::string summary;
    };
    
    const Statistics& get_statistics() const { return stats; }
    
private:
    struct StubPlacement {
        size_t segment_index;
        uint64_t file_offset;
        uint64_t vaddr;
        uint64_t capacity;
    };

    ElfParser parser;
    SegmentHandler* segment_handler;
    StubInjector* stub_injector;
    OffsetCalculator offset_calc;
    
    std::vector<uint8_t> processed_data;
    ElfSplitterOptions options;
    Statistics stats;
    std::string last_error;
    
    // 记录插入位置和大小
    std::vector<uint64_t> insert_offsets;    // 文件偏移（原始坐标，供静态修复使用）
    std::vector<uint64_t> insert_sizes;
    std::vector<uint64_t> insert_vaddrs;     // 插入块的虚拟地址（扩展后坐标，供运行时 stub 使用）
    std::vector<uint64_t> insert_bytes_to_move; // 每个插入块之后需要移动的字节数
    std::vector<bool> insert_in_exec_seg;    // 是否位于可执行段
    std::vector<uint64_t> segment_size_increase;
    std::vector<StubPlacement> stub_placements;
    
    // 内部处理函数
    bool process_pt_load_segments();
    bool process_segment_gaps();
    bool rebuild_elf_headers();
    bool validate_boundaries();
    bool inject_stub();
    uint64_t map_relative_offset(const SegmentProcessResult& result, uint64_t relative_offset) const;
    uint64_t calculate_protected_prefix(const SegmentInfo& seg) const;
    uint64_t find_next_blocking_offset(size_t current_segment_index) const;
    bool update_program_header_sizes(size_t segment_index, const SegmentInfo& seg, uint64_t inserted_bytes);
    bool update_entry_point(const SegmentInfo& seg, const SegmentProcessResult& result);
    bool update_section_headers(const SegmentInfo& seg, const SegmentProcessResult& result);
    bool apply_restore_repairs_for_executable_segments();
    bool write_entry_point(uint64_t entry);
};

#endif // __ELF_SPLITTER_H__

#ifndef __ELF_SPLITTER_H__
#define __ELF_SPLITTER_H__

#include "elf_parser.h"
#include "segment_handler.h"
#include "offset_calculator.h"
#include "common.h"
#include <vector>
#include <string>

// ELF 分割和插入处理器
class ElfSplitter {
public:
    ElfSplitter();
    ~ElfSplitter();
    
    // 设置处理选项
    void set_options(size_t interval, size_t insert_size, bool use_nop, Architecture arch);
    
    // 加载 ELF 文件
    bool load_elf(const std::string& filename);
    
    // 处理 ELF 文件
    bool process();
    
    // 保存处理后的 ELF 文件
    bool save_elf(const std::string& output_filename);
    
    // 分析文件（不处理）
    bool analyze();
    
    // 获取错误信息
    const std::string& get_last_error() const { return last_error; }
    
    // 获取处理统计信息
    struct Statistics {
        size_t total_segments_processed;
        size_t total_bytes_inserted;
        size_t segments_skipped;
        std::string summary;
    };
    
    const Statistics& get_statistics() const { return stats; }
    
private:
    ElfParser parser;
    SegmentHandler* segment_handler;
    OffsetCalculator offset_calc;
    
    std::vector<uint8_t> processed_data;
    ProcessOptions options;
    Statistics stats;
    std::string last_error;
    
    // 内部处理函数
    bool process_pt_load_segments();
    bool process_segment_gaps();
    bool rebuild_elf_headers();
    bool validate_boundaries();
};

#endif // __ELF_SPLITTER_H__
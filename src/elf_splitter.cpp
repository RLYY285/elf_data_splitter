#include "elf_splitter.h"
#include <cstring>
#include <algorithm>

ElfSplitter::ElfSplitter() : segment_handler(nullptr) {
    std::memset(&stats, 0, sizeof(stats));
}

ElfSplitter::~ElfSplitter() {
    if (segment_handler) {
        delete segment_handler;
        segment_handler = nullptr;
    }
}

void ElfSplitter::set_options(size_t interval, size_t insert_size, bool use_nop, Architecture arch) {
    options.interval = interval;
    options.insert_size = insert_size;
    options.use_nop = use_nop;
    options.arch = arch;
}

bool ElfSplitter::load_elf(const std::string& filename) {
    if (!parser.load(filename)) {
        last_error = parser.get_last_error();
        return false;
    }
    
    log_info("Successfully loaded ELF file: " + filename);
    log_info("Type: " + std::string(parser.is_executable() ? "ET_EXEC" : 
                                   (parser.is_shared_object() ? "ET_DYN" : "ET_UNKNOWN")));
    log_info("Architecture: " + std::to_string(parser.get_machine()));
    
    return true;
}

bool ElfSplitter::analyze() {
    log_info("=== ELF File Analysis ===");
    log_info("Is 64-bit: " + std::string(parser.is_64bit() ? "Yes" : "No"));
    log_info("Is Little-Endian: " + std::string(parser.is_little_endian() ? "Yes" : "No"));
    log_info("Entry Point: 0x" + std::to_string(parser.get_entry()));
    log_info("Number of Program Headers: " + std::to_string(parser.get_phnum()));
    log_info("Number of Section Headers: " + std::to_string(parser.get_shnum()));
    
    log_info("\n=== Program Segments ===");
    const auto& segments = parser.get_segments();
    for (size_t i = 0; i < segments.size(); ++i) {
        const auto& seg = segments[i];
        std::string type_str;
        switch (seg.p_type) {
            case PT_LOAD: type_str = "PT_LOAD"; break;
            case PT_DYNAMIC: type_str = "PT_DYNAMIC"; break;
            case PT_INTERP: type_str = "PT_INTERP"; break;
            case PT_NOTE: type_str = "PT_NOTE"; break;
            case PT_PHDR: type_str = "PT_PHDR"; break;
            default: type_str = "UNKNOWN(" + std::to_string(seg.p_type) + ")"; break;
        }
        
        log_info("Segment " + std::to_string(i) + ": " + type_str);
        log_info("  Offset: 0x" + std::to_string(seg.p_offset) + ", Size: " + std::to_string(seg.p_filesz));
        log_info("  VAddr: 0x" + std::to_string(seg.p_vaddr) + ", Flags: " + std::to_string(seg.p_flags));
    }
    
    return true;
}

bool ElfSplitter::process() {
    if (!segment_handler) {
        segment_handler = new SegmentHandler(options);
    }
    
    log_info("Starting ELF processing...");
    log_info("Interval: " + std::to_string(options.interval) + " bytes");
    log_info("Insert Size: " + std::to_string(options.insert_size) + " bytes");
    log_info("Fill Mode: " + std::string(options.use_nop ? "NOP" : "Zero"));
    
    // 复制原始数据
    processed_data = parser.get_file_data();
    
    // 处理 PT_LOAD 段
    if (!process_pt_load_segments()) {
        return false;
    }
    
    // 处理段间隙
    if (!process_segment_gaps()) {
        return false;
    }
    
    // 重建 ELF 头
    if (!rebuild_elf_headers()) {
        return false;
    }
    
    // 验证边界
    if (!validate_boundaries()) {
        return false;
    }
    
    log_info("Processing completed successfully");
    stats.summary = "Processed " + std::to_string(stats.total_segments_processed) + 
                   " segments, inserted " + std::to_string(stats.total_bytes_inserted) + " bytes";
    log_info(stats.summary);
    
    return true;
}

bool ElfSplitter::process_pt_load_segments() {
    const auto& segments = parser.get_segments();
    
    for (size_t i = 0; i < segments.size(); ++i) {
        if (!segments[i].is_load()) {
            continue;
        }
        
        // 确定下一个段的起始位置
        uint64_t next_segment_offset = UINT64_MAX;
        for (size_t j = i + 1; j < segments.size(); ++j) {
            if (segments[j].p_offset > segments[i].p_offset) {
                next_segment_offset = segments[j].p_offset;
                break;
            }
        }
        
        // 处理段
        const auto& seg = segments[i];
        SegmentProcessResult result = segment_handler->process_segment(
            processed_data,
            seg.p_offset,
            seg.p_filesz,
            next_segment_offset
        );
        
        if (!result.success) {
            log_error("Failed to process segment " + std::to_string(i));
            stats.segments_skipped++;
            continue;
        }
        
        // 插入处理后的数据
        if (result.inserted_bytes > 0) {
            // 这里需要实际替换数据并更新后续偏移
            log_info("Segment " + std::to_string(i) + ": inserted " + 
                    std::to_string(result.inserted_bytes) + " bytes");
            stats.total_bytes_inserted += result.inserted_bytes;
            stats.total_segments_processed++;
        }
    }
    
    return true;
}

bool ElfSplitter::process_segment_gaps() {
    // TODO: 实现段间隙处理
    return true;
}

bool ElfSplitter::rebuild_elf_headers() {
    // TODO: 根据插入的数据重新计算并更新 ELF 头的偏移
    return true;
}

bool ElfSplitter::validate_boundaries() {
    const auto& segments = parser.get_segments();
    
    for (size_t i = 0; i < segments.size() - 1; ++i) {
        uint64_t current_end = segments[i].get_end_offset();
        uint64_t next_start = segments[i + 1].p_offset;
        
        if (current_end > next_start) {
            last_error = "Segment boundary violation: segment " + std::to_string(i) + 
                        " extends beyond segment " + std::to_string(i + 1);
            return false;
        }
    }
    
    return true;
}

bool ElfSplitter::save_elf(const std::string& output_filename) {
    // TODO: 保存处理后的 ELF 文件
    return true;
}
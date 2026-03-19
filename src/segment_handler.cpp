#include "segment_handler.h"
#include <cstring>
#include <algorithm>

SegmentHandler::SegmentHandler(const ProcessOptions& opts)
    : options(opts) {
    if (opts.use_nop) {
        nop_info = get_nop_info(opts.arch);
        if (!nop_info) {
            log_warn("No NOP information for architecture, using zero fill");
        }
    } else {
        nop_info = nullptr;
    }
}

SegmentHandler::~SegmentHandler() {}

void SegmentHandler::fill_insert_data(uint8_t* buffer, size_t size) const {
    if (nop_info && nop_info->nop_size <= size) {
        // 重复填充 NOP 指令
        size_t pos = 0;
        while (pos + nop_info->nop_size <= size) {
            std::memcpy(buffer + pos, nop_info->nop_bytes, nop_info->nop_size);
            pos += nop_info->nop_size;
        }
        // 剩余字节用 0 填充
        std::memset(buffer + pos, 0, size - pos);
    } else {
        // 使用 0 填充
        std::memset(buffer, 0, size);
    }
}

uint64_t SegmentHandler::calculate_new_size(uint64_t original_size, uint64_t available_space) const {
    if (original_size == 0 || options.interval == 0) {
        return original_size;
    }
    
    // 计算可以进行多少次插入
    uint64_t num_intervals = original_size / options.interval;
    uint64_t total_insertions = num_intervals * options.insert_size;
    uint64_t new_size = original_size + total_insertions;
    
    // 检查是否超出可用空间
    if (new_size > available_space) {
        // 动态减少插入大小
        uint64_t max_insertions = available_space - original_size;
        uint64_t max_insert_per_interval = max_insertions / num_intervals;
        new_size = original_size + (num_intervals * max_insert_per_interval);
    }
    
    return new_size;
}

SegmentProcessResult SegmentHandler::process_segment(
    const std::vector<uint8_t>& data,
    uint64_t offset,
    uint64_t size,
    uint64_t next_segment_offset) {
    
    SegmentProcessResult result;
    result.original_size = size;
    result.success = false;
    
    if (size == 0 || options.interval == 0) {
        result.processed_data = std::vector<uint8_t>(data.begin() + offset, data.begin() + offset + size);
        result.new_size = size;
        result.inserted_bytes = 0;
        result.success = true;
        return result;
    }
    
    // 计算可用空间
    uint64_t available_space = UINT64_MAX;
    if (next_segment_offset != UINT64_MAX) {
        available_space = next_segment_offset - offset;
    }
    
    // 计算可以插入的总大小
    uint64_t num_intervals = size / options.interval;
    uint64_t total_insert_size = num_intervals * options.insert_size;
    
    // 检查是否会超出下一个段
    if (offset + size + total_insert_size > next_segment_offset) {
        // 动态调整插入大小
        uint64_t max_additional = available_space - size;
        total_insert_size = std::min(total_insert_size, max_additional);
        
        if (total_insert_size == 0) {
            log_warn("Cannot insert data due to segment boundary constraints");
            result.processed_data = std::vector<uint8_t>(data.begin() + offset, data.begin() + offset + size);
            result.new_size = size;
            result.inserted_bytes = 0;
            result.success = true;
            return result;
        }
    }
    
    result.new_size = size + total_insert_size;
    result.inserted_bytes = total_insert_size;
    result.processed_data.reserve(result.new_size);
    
    // 执行切割和插入
    uint64_t pos = 0;
    uint64_t actual_insert_size = total_insert_size;
    if (num_intervals > 0) {
        actual_insert_size = total_insert_size / num_intervals;
    }
    
    for (uint64_t i = 0; i < size; ++i) {
        result.processed_data.push_back(data[offset + i]);
        
        // 检查是否需要插入
        if ((i + 1) % options.interval == 0 && (i + 1) < size) {
            uint64_t insert_bytes = std::min(options.insert_size, 
                                            total_insert_size - (result.processed_data.size() - size));
            for (uint64_t j = 0; j < insert_bytes; ++j) {
                uint8_t byte = 0;
                fill_insert_data(&byte, 1);
                result.processed_data.push_back(byte);
            }
        }
    }
    
    result.success = true;
    return result;
}
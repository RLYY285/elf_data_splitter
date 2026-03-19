#ifndef __SEGMENT_HANDLER_H__
#define __SEGMENT_HANDLER_H__

#include "common.h"
#include "elf_parser.h"
#include <vector>
#include <cstdint>

// 处理选项
struct ProcessOptions {
    size_t interval;           // 每隔多少字节插入数据
    size_t insert_size;        // 每次插入多少字节
    bool use_nop;              // 是否使用 NOP 填充，否则使用 0
    Architecture arch;         // 目标架构
    bool auto_optimize;        // 自动求解最优插入参数
};

// 段处理结果
struct SegmentProcessResult {
    std::vector<uint8_t> processed_data;  // 处理后的数据
    uint64_t original_size;               // 原始大小
    uint64_t new_size;                    // 处理后的大小
    uint64_t inserted_bytes;              // 插入的字节数
    uint64_t interval_used;               // 实际使用的间隔
    uint64_t insertion_events;            // 实际插入点数量
    std::vector<uint64_t> insertion_points; // 相对段起点的插入点（插入发生在该偏移后）
    std::vector<uint64_t> insertion_sizes;  // 每个插入点的插入字节数
    bool success;                         // 处理是否成功
    std::string error_msg;                // 错误信息
};

// 段处理器
class SegmentHandler {
public:
    SegmentHandler(const ProcessOptions& options);
    ~SegmentHandler();
    
    // 处理单个段
    SegmentProcessResult process_segment(
        const std::vector<uint8_t>& data,
        uint64_t offset,
        uint64_t size,
        uint64_t next_segment_offset = UINT64_MAX,
        uint64_t protected_prefix = 0
    );
    
    // 计算处理后的大小（不实际处理）
    uint64_t calculate_new_size(uint64_t original_size, uint64_t available_space) const;
    
private:
    struct OptimalParams {
        uint64_t interval;
        uint64_t insert_size;
        uint64_t insertion_events;
        uint64_t total_insert_size;
    };

    ProcessOptions options;
    const ArchNOPInfo* nop_info;
    
    // 内部处理函数
    void fill_insert_data(uint8_t* buffer, size_t size) const;
    OptimalParams calculate_optimal_params(uint64_t payload_size, uint64_t max_insert_size) const;
};

#endif // __SEGMENT_HANDLER_H__

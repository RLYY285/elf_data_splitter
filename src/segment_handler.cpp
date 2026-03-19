#include "segment_handler.h"
#include <cstring>
#include <algorithm>
#include <limits>

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

SegmentHandler::OptimalParams SegmentHandler::calculate_optimal_params(
    uint64_t payload_size,
    uint64_t max_insert_size) const {
    OptimalParams best{0, 0, 0, 0};
    if (payload_size < 2 || max_insert_size == 0) {
        return best;
    }

    // 目标：在给定可用空间内，找到使总插入字节最大的 interval/insert_size 组合；
    // 若总量相同，优先更多插入点（更均匀分布）。
    for (uint64_t interval = 1; interval < payload_size; ++interval) {
        const uint64_t insertion_events = (payload_size - 1) / interval;
        if (insertion_events == 0) {
            continue;
        }

        const uint64_t insert_size = max_insert_size / insertion_events;
        if (insert_size == 0) {
            continue;
        }

        const uint64_t total_insert_size = insert_size * insertion_events;
        if (total_insert_size > best.total_insert_size ||
            (total_insert_size == best.total_insert_size &&
             insertion_events > best.insertion_events)) {
            best.interval = interval;
            best.insert_size = insert_size;
            best.insertion_events = insertion_events;
            best.total_insert_size = total_insert_size;
        }
    }

    return best;
}

uint64_t SegmentHandler::calculate_new_size(uint64_t original_size, uint64_t available_space) const {
    if (original_size == 0 || available_space <= original_size) {
        return original_size;
    }

    const uint64_t max_insert = available_space - original_size;
    if (max_insert == 0) {
        return original_size;
    }

    uint64_t interval = options.interval;
    uint64_t insert_size = options.insert_size;
    uint64_t insertion_events = 0;
    uint64_t total_insertions = 0;

    if (options.auto_optimize || interval == 0 || insert_size == 0) {
        OptimalParams best = calculate_optimal_params(original_size, max_insert);
        interval = best.interval;
        insert_size = best.insert_size;
        insertion_events = best.insertion_events;
        total_insertions = best.total_insert_size;
    } else {
        interval = std::max<uint64_t>(1, interval);
        insertion_events = (original_size - 1) / interval;
        total_insertions = insertion_events * insert_size;
        total_insertions = std::min(total_insertions, max_insert);
    }

    if (interval == 0 || insert_size == 0 || insertion_events == 0 || total_insertions == 0) {
        return original_size;
    }

    return original_size + total_insertions;
}

SegmentProcessResult SegmentHandler::process_segment(
    const std::vector<uint8_t>& data,
    uint64_t offset,
    uint64_t size,
    uint64_t next_segment_offset,
    uint64_t protected_prefix) {

    SegmentProcessResult result{};
    result.original_size = size;
    result.new_size = size;
    result.inserted_bytes = 0;
    result.interval_used = 0;
    result.insertion_events = 0;
    result.success = false;

    if (offset > data.size() || size > data.size() - offset) {
        result.error_msg = "Segment offset/size is out of file bounds";
        return result;
    }

    protected_prefix = std::min<uint64_t>(protected_prefix, size);
    const uint64_t payload_size = size - protected_prefix;

    if (size == 0 || payload_size == 0) {
        result.processed_data.assign(data.begin() + offset, data.begin() + offset + size);
        result.success = true;
        return result;
    }

    uint64_t available_space = data.size() - offset;
    if (next_segment_offset != UINT64_MAX) {
        if (next_segment_offset <= offset) {
            available_space = 0;
        } else {
            available_space = std::min<uint64_t>(available_space, next_segment_offset - offset);
        }
    }

    if (available_space <= size) {
        result.processed_data.assign(data.begin() + offset, data.begin() + offset + size);
        result.success = true;
        return result;
    }

    const uint64_t max_insert_size = available_space - size;
    uint64_t interval = options.interval;
    uint64_t insert_size = options.insert_size;
    uint64_t insertion_events = 0;
    uint64_t total_insert_size = 0;

    if (options.auto_optimize || interval == 0 || insert_size == 0) {
        OptimalParams best = calculate_optimal_params(payload_size, max_insert_size);
        interval = best.interval;
        insert_size = best.insert_size;
        insertion_events = best.insertion_events;
        total_insert_size = best.total_insert_size;
    } else {
        interval = std::max<uint64_t>(1, interval);
        insertion_events = (payload_size - 1) / interval;
        total_insert_size = insertion_events * insert_size;
        total_insert_size = std::min(total_insert_size, max_insert_size);
    }

    if (interval == 0 || insert_size == 0 || insertion_events == 0 || total_insert_size == 0) {
        result.processed_data.assign(data.begin() + offset, data.begin() + offset + size);
        result.success = true;
        return result;
    }

    result.interval_used = interval;
    result.insertion_events = insertion_events;
    result.inserted_bytes = total_insert_size;
    result.new_size = size + total_insert_size;
    result.processed_data.reserve(result.new_size);

    // 先保留受保护前缀，避免破坏 ELF/PHDR/SHDR 等元数据所在区域。
    result.processed_data.insert(
        result.processed_data.end(),
        data.begin() + offset,
        data.begin() + offset + protected_prefix);

    const uint64_t base_insert_size = total_insert_size / insertion_events;
    const uint64_t remainder = total_insert_size % insertion_events;
    uint64_t event_idx = 0;

    for (uint64_t i = 0; i < payload_size; ++i) {
        result.processed_data.push_back(data[offset + protected_prefix + i]);

        if ((i + 1) % interval == 0 && (i + 1) < payload_size && event_idx < insertion_events) {
            const uint64_t insert_now = base_insert_size + ((event_idx < remainder) ? 1 : 0);
            if (insert_now > 0) {
                const size_t start = result.processed_data.size();
                result.processed_data.resize(start + static_cast<size_t>(insert_now));
                fill_insert_data(result.processed_data.data() + start, static_cast<size_t>(insert_now));
                result.insertion_points.push_back(protected_prefix + i + 1);
                result.insertion_sizes.push_back(insert_now);
            }
            ++event_idx;
        }
    }

    result.success = true;
    return result;
}

#include "elf_splitter.h"
#include <cstring>
#include <algorithm>
#include <fstream>
#include <limits>

ElfSplitter::ElfSplitter() : segment_handler(nullptr), stub_injector(nullptr) {
    options.interval = 0;
    options.insert_size = 0;
    options.use_nop = false;
    options.arch = Architecture::UNKNOWN;
    options.auto_optimize = true;

    stats.total_segments_processed = 0;
    stats.total_bytes_inserted = 0;
    stats.segments_skipped = 0;
    stats.stub_injected = false;
    stats.stub_offset = 0;
    stats.stub_size = 0;
    stats.new_entry_point = 0;
    stats.summary = "";
}

ElfSplitter::~ElfSplitter() {
    if (segment_handler) {
        delete segment_handler;
        segment_handler = nullptr;
    }
    if (stub_injector) {
        delete stub_injector;
        stub_injector = nullptr;
    }
}

void ElfSplitter::set_options(const ElfSplitterOptions& opts) {
    options = opts;
}

void ElfSplitter::set_options(
    size_t interval,
    size_t insert_size,
    bool use_nop,
    Architecture arch,
    bool auto_optimize) {
    options.interval = interval;
    options.insert_size = insert_size;
    options.use_nop = use_nop;
    options.arch = arch;
    options.auto_optimize = auto_optimize;
    options.allow_exec_segment_edit = true;
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
    if (parser.get_file_data().empty()) {
        last_error = "No ELF file loaded";
        return false;
    }

    stats.total_segments_processed = 0;
    stats.total_bytes_inserted = 0;
    stats.segments_skipped = 0;
    stats.stub_injected = false;
    stats.stub_offset = 0;
    stats.stub_size = 0;
    stats.new_entry_point = 0;
    stats.summary.clear();
    last_error.clear();
    insert_offsets.clear();
    insert_sizes.clear();
    stub_placements.clear();

    if (options.arch == Architecture::UNKNOWN) {
        options.arch = get_architecture(parser.get_machine());
    }

    if (segment_handler) {
        delete segment_handler;
        segment_handler = nullptr;
    }
    ProcessOptions process_options{};
    process_options.interval = options.interval;
    process_options.insert_size = options.insert_size;
    process_options.use_nop = options.use_nop;
    process_options.arch = options.arch;
    process_options.auto_optimize = options.auto_optimize;
    segment_handler = new SegmentHandler(process_options);

    if (!segment_handler) {
        last_error = "Failed to initialize segment handler";
        return false;
    }
    
    log_info("Starting ELF processing...");
    if (options.auto_optimize || options.interval == 0 || options.insert_size == 0) {
        log_info("Mode: auto optimize interval/insert-size per PT_LOAD segment");
    } else {
        log_info("Interval: " + std::to_string(options.interval) + " bytes");
        log_info("Insert Size: " + std::to_string(options.insert_size) + " bytes");
    }
    log_info("Fill Mode: " + std::string(options.use_nop ? "NOP" : "Zero"));
    
    // 复制原始数据
    processed_data = parser.get_file_data();
    segment_size_increase.assign(parser.get_segments().size(), 0);
    
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

    // 注入 stub（可选）
    if (options.inject_stub) {
        if (!inject_stub()) {
            return false;
        }
    }
    
    log_info("Processing completed successfully");
    stats.summary = "Processed " + std::to_string(stats.total_segments_processed) +
                   " segments, inserted " + std::to_string(stats.total_bytes_inserted) + " bytes";
    if (stats.stub_injected) {
        stats.summary += ", stub injected at 0x" + std::to_string(stats.stub_offset) +
                        " (" + std::to_string(stats.stub_size) + " bytes)";
    }
    log_info(stats.summary);
    
    return true;
}

bool ElfSplitter::process_pt_load_segments() {
    const auto& segments = parser.get_segments();
    std::vector<size_t> load_indices;
    for (size_t i = 0; i < segments.size(); ++i) {
        if (segments[i].is_load()) {
            load_indices.push_back(i);
        }
    }

    std::sort(load_indices.begin(), load_indices.end(),
              [&segments](size_t lhs, size_t rhs) {
                  return segments[lhs].p_offset < segments[rhs].p_offset;
              });

    auto find_next_load_offset = [&segments, this](size_t current_idx) -> uint64_t {
        const uint64_t current_offset = segments[current_idx].p_offset;
        uint64_t next_offset = processed_data.size();
        for (size_t i = 0; i < segments.size(); ++i) {
            if (i == current_idx || !segments[i].is_load()) {
                continue;
            }
            if (segments[i].p_offset > current_offset) {
                next_offset = std::min<uint64_t>(next_offset, segments[i].p_offset);
            }
        }
        return next_offset;
    };
    
    for (size_t idx : load_indices) {
        const auto& seg = segments[idx];
        if (seg.is_executable() && !options.allow_exec_segment_edit) {
            log_info("Segment " + std::to_string(idx) + ": skipped executable PT_LOAD segment");
            stats.segments_skipped++;
            continue;
        }

        const uint64_t next_segment_offset = find_next_blocking_offset(idx);
        const uint64_t next_load_offset = find_next_load_offset(idx);
        const uint64_t hard_limit = std::min<uint64_t>(next_segment_offset, next_load_offset);
        const uint64_t protected_prefix = calculate_protected_prefix(seg);

        // 处理段
        SegmentProcessResult result = segment_handler->process_segment(
            processed_data,
            seg.p_offset,
            seg.p_filesz,
            hard_limit,
            protected_prefix
        );
        
        if (!result.success) {
            log_error("Failed to process segment " + std::to_string(idx) + ": " + result.error_msg);
            last_error = "Failed to process PT_LOAD segment " + std::to_string(idx);
            stats.segments_skipped++;
            continue;
        }

        if (seg.p_offset + result.new_size > processed_data.size()) {
            last_error = "Processed segment exceeds current file buffer";
            return false;
        }
        if (seg.p_offset + result.new_size > next_load_offset) {
            last_error = "Processed segment exceeds next PT_LOAD boundary";
            return false;
        }

        std::copy(
            result.processed_data.begin(),
            result.processed_data.end(),
            processed_data.begin() + static_cast<std::ptrdiff_t>(seg.p_offset));

        if (!update_program_header_sizes(idx, seg, result.inserted_bytes)) {
            return false;
        }

        if (!update_entry_point(seg, result)) {
            return false;
        }

        if (!update_section_headers(seg, result)) {
            return false;
        }

        segment_size_increase[idx] = result.inserted_bytes;
        for (size_t i = 0; i < result.insertion_points.size(); ++i) {
            insert_offsets.push_back(seg.p_offset + result.insertion_points[i]);
            insert_sizes.push_back(result.insertion_sizes[i]);
        }
        
        if (result.inserted_bytes > 0) {
            log_info("Segment " + std::to_string(idx) +
                     ": inserted " + std::to_string(result.inserted_bytes) +
                     " bytes (interval=" + std::to_string(result.interval_used) +
                     ", events=" + std::to_string(result.insertion_events) + ")");
            stats.total_bytes_inserted += result.inserted_bytes;
            stats.total_segments_processed++;
        } else {
            log_info("Segment " + std::to_string(idx) + ": no insertion (insufficient safe space)");
            stats.segments_skipped++;
        }
    }
    
    return true;
}

bool ElfSplitter::process_segment_gaps() {
    // TODO: 实现段间隙处理
    return true;
}

bool ElfSplitter::rebuild_elf_headers() {
    // 当前策略在段内扩展并即时更新受影响头字段，无需整体重排。
    return true;
}

bool ElfSplitter::validate_boundaries() {
    const auto& segments = parser.get_segments();

    struct LoadBoundary {
        size_t index;
        uint64_t start;
        uint64_t end;
    };

    std::vector<LoadBoundary> boundaries;
    boundaries.reserve(segments.size());
    for (size_t i = 0; i < segments.size(); ++i) {
        if (!segments[i].is_load()) {
            continue;
        }
        uint64_t end = segments[i].p_offset + segments[i].p_filesz + segment_size_increase[i];
        boundaries.push_back({i, segments[i].p_offset, end});
    }

    std::sort(boundaries.begin(), boundaries.end(),
              [](const LoadBoundary& lhs, const LoadBoundary& rhs) {
                  return lhs.start < rhs.start;
              });

    for (size_t i = 1; i < boundaries.size(); ++i) {
        const auto& prev = boundaries[i - 1];
        const auto& curr = boundaries[i];

        if (prev.end > curr.start) {
            last_error = "PT_LOAD boundary violation: segment " + std::to_string(prev.index) +
                         " overlaps segment " + std::to_string(curr.index);
            return false;
        }
    }
    
    return true;
}

bool ElfSplitter::save_elf(const std::string& output_filename) {
    if (processed_data.empty()) {
        last_error = "No processed data to save";
        return false;
    }

    std::ofstream out(output_filename, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        last_error = "Failed to open output file: " + output_filename;
        return false;
    }

    out.write(reinterpret_cast<const char*>(processed_data.data()),
              static_cast<std::streamsize>(processed_data.size()));
    if (!out.good()) {
        last_error = "Failed to write output file: " + output_filename;
        return false;
    }

    out.close();
    if (!out.good()) {
        last_error = "Failed to finalize output file: " + output_filename;
        return false;
    }

    return true;
}

bool ElfSplitter::write_entry_point(uint64_t entry) {
    const bool is_le = parser.is_little_endian();
    if (parser.is_64bit()) {
        if (processed_data.size() < sizeof(Elf64_Ehdr)) {
            last_error = "ELF header out of bounds while writing entry";
            return false;
        }
        auto* ehdr = reinterpret_cast<Elf64_Ehdr*>(processed_data.data());
        write_u64(reinterpret_cast<uint8_t*>(&ehdr->e_entry), entry, is_le);
        return true;
    }

    if (entry > std::numeric_limits<uint32_t>::max()) {
        last_error = "32-bit entry value overflow";
        return false;
    }
    if (processed_data.size() < sizeof(Elf32_Ehdr)) {
        last_error = "ELF header out of bounds while writing entry";
        return false;
    }
    auto* ehdr = reinterpret_cast<Elf32_Ehdr*>(processed_data.data());
    write_u32(reinterpret_cast<uint8_t*>(&ehdr->e_entry), static_cast<uint32_t>(entry), is_le);
    return true;
}

bool ElfSplitter::apply_restore_repairs_for_executable_segments() {
    const auto& segments = parser.get_segments();
    stub_placements.clear();

    for (size_t idx = 0; idx < segments.size(); ++idx) {
        const auto& seg = segments[idx];
        if (!seg.is_load() || !seg.is_executable() || segment_size_increase[idx] == 0) {
            continue;
        }

        const uint64_t seg_start = seg.p_offset;
        const uint64_t seg_end = seg.p_offset + seg.p_filesz;
        const uint64_t expanded_size = seg.p_filesz + segment_size_increase[idx];

        struct Insertion {
            uint64_t rel_offset;
            uint64_t size;
        };
        std::vector<Insertion> repairs;
        repairs.reserve(insert_offsets.size());
        for (size_t i = 0; i < insert_offsets.size(); ++i) {
            const uint64_t off = insert_offsets[i];
            if (off >= seg_start && off < seg_end) {
                repairs.push_back({off - seg_start, insert_sizes[i]});
            }
        }

        if (repairs.empty()) {
            continue;
        }

        std::sort(repairs.begin(), repairs.end(),
                  [](const Insertion& lhs, const Insertion& rhs) {
                      return lhs.rel_offset < rhs.rel_offset;
                  });

        uint64_t current_size = expanded_size;
        for (const auto& ins : repairs) {
            if (ins.rel_offset + ins.size > current_size) {
                last_error = "Restore repair insertion range out of bounds";
                return false;
            }

            uint8_t* base = processed_data.data() + seg_start;
            const uint64_t src_off = ins.rel_offset + ins.size;
            const uint64_t move_len = current_size - src_off;
            std::memmove(base + ins.rel_offset, base + src_off, static_cast<size_t>(move_len));
            current_size -= ins.size;
        }

        if (seg_start + expanded_size > processed_data.size()) {
            last_error = "Restore repair exceeds file bounds";
            return false;
        }

        uint8_t* base = processed_data.data() + seg_start;
        std::memset(base + current_size, 0, static_cast<size_t>(expanded_size - current_size));
        const uint64_t slack_size = expanded_size - current_size;
        if (slack_size > 0) {
            StubPlacement placement{};
            placement.segment_index = idx;
            placement.file_offset = seg_start + current_size;
            placement.vaddr = seg.p_vaddr + current_size;
            placement.capacity = slack_size;
            stub_placements.push_back(placement);
        }
        log_info("Repaired executable segment " + std::to_string(idx) +
                 " using restore metadata (" + std::to_string(slack_size) +
                 " bytes compacted)");
    }

    if (!write_entry_point(parser.get_entry())) {
        return false;
    }

    return true;
}

bool ElfSplitter::inject_stub() {
    if (!options.inject_stub) {
        return true;
    }

    if (stub_injector) {
        delete stub_injector;
        stub_injector = nullptr;
    }
    stub_injector = new StubInjector(parser, options.arch);
    if (!stub_injector) {
        last_error = "Failed to initialize stub injector";
        return false;
    }

    StubInjectionOptions stub_options{};
    stub_options.inject_stub = true;
    stub_options.stub_type = options.stub_type;
    stub_options.stub_base_offset = processed_data.size();
    stub_options.stub_base_vaddr = 0;
    stub_options.stub_region_size = 0;
    stub_options.has_fixed_stub_base = false;
    stub_options.update_entry_point = options.update_entry_point;
    stub_options.preserve_original_entry = true;

    bool success = false;
    switch (options.stub_type) {
        case StubType::RESTORE_DATA:
            if (options.update_entry_point && options.arch != Architecture::X86_64) {
                last_error = "stub-update-entry currently supports x86_64 restore only";
                return false;
            }
            if (!apply_restore_repairs_for_executable_segments()) {
                return false;
            }
            if (options.update_entry_point) {
                if (stub_placements.empty()) {
                    last_error = "No executable slack region available for entry stub";
                    return false;
                }
                const auto best = std::max_element(
                    stub_placements.begin(), stub_placements.end(),
                    [](const StubPlacement& lhs, const StubPlacement& rhs) {
                        return lhs.capacity < rhs.capacity;
                    });
                stub_options.stub_base_offset = best->file_offset;
                stub_options.stub_base_vaddr = best->vaddr;
                stub_options.stub_region_size = best->capacity;
                stub_options.has_fixed_stub_base = true;
            }
            // apply_restore_repairs_for_executable_segments() 静态修复了可执行段的插入数据；
            // stub 元数据只需包含非可执行段的插入，避免对已修复区域二次操作。
            {
                std::vector<uint64_t> non_exec_offsets;
                std::vector<uint64_t> non_exec_sizes;
                const auto& segs = parser.get_segments();
                for (size_t i = 0; i < insert_offsets.size(); ++i) {
                    bool in_exec = false;
                    for (const auto& seg : segs) {
                        if (!seg.is_load() || !seg.is_executable()) {
                            continue;
                        }
                        if (insert_offsets[i] >= seg.p_offset &&
                            insert_offsets[i] < seg.p_offset + seg.p_filesz) {
                            in_exec = true;
                            break;
                        }
                    }
                    if (!in_exec) {
                        non_exec_offsets.push_back(insert_offsets[i]);
                        non_exec_sizes.push_back(insert_sizes[i]);
                    }
                }
                success = stub_injector->inject_restore_stub(
                    processed_data, non_exec_offsets, non_exec_sizes, stub_options);
            }
            break;
        case StubType::CUSTOM:
            if (options.custom_stub_code.empty()) {
                last_error = "Custom stub code is empty";
                return false;
            }
            if (options.update_entry_point) {
                last_error = "stub-update-entry currently supports restore stub only";
                return false;
            }
            success = stub_injector->inject_custom_stub(
                processed_data, options.custom_stub_code, stub_options);
            break;
        case StubType::DECRYPT:
        case StubType::DECOMPRESS:
            last_error = "Selected stub type is not implemented yet";
            return false;
    }

    if (!success) {
        last_error = "Stub injection failed";
        return false;
    }

    stats.stub_injected = true;
    stats.stub_offset = stub_injector->get_stub_offset();
    stats.stub_size = stub_injector->get_stub_size();
    stats.new_entry_point = stub_injector->get_new_entry_point();
    log_info("Stub injection completed: offset=0x" + std::to_string(stats.stub_offset) +
             ", size=" + std::to_string(stats.stub_size));
    if (options.update_entry_point) {
        log_info("Stub entry point set to 0x" + std::to_string(stats.new_entry_point));
    }

    return true;
}

uint64_t ElfSplitter::map_relative_offset(const SegmentProcessResult& result, uint64_t relative_offset) const {
    uint64_t delta = 0;
    for (size_t i = 0; i < result.insertion_points.size(); ++i) {
        if (result.insertion_points[i] <= relative_offset) {
            delta += result.insertion_sizes[i];
        } else {
            break;
        }
    }
    return relative_offset + delta;
}

uint64_t ElfSplitter::calculate_protected_prefix(const SegmentInfo& seg) const {
    const uint64_t seg_start = seg.p_offset;
    const uint64_t seg_end = seg.p_offset + seg.p_filesz;
    uint64_t protected_prefix = 0;

    auto safe_add = [](uint64_t a, uint64_t b) -> uint64_t {
        if (a > std::numeric_limits<uint64_t>::max() - b) {
            return std::numeric_limits<uint64_t>::max();
        }
        return a + b;
    };

    auto protect_range = [&](uint64_t range_start, uint64_t range_size) {
        if (range_size == 0) {
            return;
        }
        const uint64_t range_end = safe_add(range_start, range_size);
        if (range_start >= seg_start && range_start < seg_end) {
            const uint64_t clipped_end = std::min<uint64_t>(range_end, seg_end);
            protected_prefix = std::max(protected_prefix, clipped_end - seg_start);
        }
    };

    const uint64_t ehdr_size = parser.is_64bit() ? sizeof(Elf64_Ehdr) : sizeof(Elf32_Ehdr);
    protect_range(0, ehdr_size);

    const uint64_t phdr_size = parser.is_64bit() ? sizeof(Elf64_Phdr) : sizeof(Elf32_Phdr);
    protect_range(parser.get_phoff(), parser.get_phnum() * phdr_size);

    if (parser.get_shoff() != 0) {
        const uint64_t shdr_size = parser.is_64bit() ? sizeof(Elf64_Shdr) : sizeof(Elf32_Shdr);
        protect_range(parser.get_shoff(), parser.get_shnum() * shdr_size);
    }

    return std::min<uint64_t>(protected_prefix, seg.p_filesz);
}

uint64_t ElfSplitter::find_next_blocking_offset(size_t current_segment_index) const {
    const auto& segments = parser.get_segments();
    const uint64_t current_offset = segments[current_segment_index].p_offset;
    uint64_t next_offset = processed_data.size();

    for (size_t i = 0; i < segments.size(); ++i) {
        if (i == current_segment_index) {
            continue;
        }
        if (segments[i].p_offset > current_offset) {
            next_offset = std::min<uint64_t>(next_offset, segments[i].p_offset);
        }
    }

    if (parser.get_shoff() > current_offset) {
        next_offset = std::min<uint64_t>(next_offset, parser.get_shoff());
    }

    return next_offset;
}

bool ElfSplitter::update_program_header_sizes(size_t segment_index, const SegmentInfo& seg, uint64_t inserted_bytes) {
    if (inserted_bytes == 0) {
        return true;
    }

    const bool is_le = parser.is_little_endian();
    const uint64_t new_filesz = seg.p_filesz + inserted_bytes;
    const uint64_t new_memsz = seg.p_memsz + inserted_bytes;

    if (parser.is_64bit()) {
        const uint64_t phoff = parser.get_phoff() + segment_index * sizeof(Elf64_Phdr);
        if (phoff + sizeof(Elf64_Phdr) > processed_data.size()) {
            last_error = "Program header out of bounds while updating size";
            return false;
        }
        auto* phdr = reinterpret_cast<Elf64_Phdr*>(processed_data.data() + phoff);
        write_u64(reinterpret_cast<uint8_t*>(&phdr->p_filesz), new_filesz, is_le);
        write_u64(reinterpret_cast<uint8_t*>(&phdr->p_memsz), new_memsz, is_le);
    } else {
        if (new_filesz > std::numeric_limits<uint32_t>::max() ||
            new_memsz > std::numeric_limits<uint32_t>::max()) {
            last_error = "32-bit program header size overflow";
            return false;
        }
        const uint64_t phoff = parser.get_phoff() + segment_index * sizeof(Elf32_Phdr);
        if (phoff + sizeof(Elf32_Phdr) > processed_data.size()) {
            last_error = "Program header out of bounds while updating size";
            return false;
        }
        auto* phdr = reinterpret_cast<Elf32_Phdr*>(processed_data.data() + phoff);
        write_u32(reinterpret_cast<uint8_t*>(&phdr->p_filesz), static_cast<uint32_t>(new_filesz), is_le);
        write_u32(reinterpret_cast<uint8_t*>(&phdr->p_memsz), static_cast<uint32_t>(new_memsz), is_le);
    }

    return true;
}

bool ElfSplitter::update_entry_point(const SegmentInfo& seg, const SegmentProcessResult& result) {
    if (result.inserted_bytes == 0) {
        return true;
    }

    const uint64_t entry = parser.get_entry();
    const uint64_t seg_vend = seg.p_vaddr + seg.p_memsz;
    if (entry < seg.p_vaddr || entry >= seg_vend) {
        return true;
    }

    const bool is_le = parser.is_little_endian();
    const uint64_t rel = entry - seg.p_vaddr;
    const uint64_t new_entry = seg.p_vaddr + map_relative_offset(result, rel);

    if (parser.is_64bit()) {
        if (processed_data.size() < sizeof(Elf64_Ehdr)) {
            last_error = "ELF header out of bounds while updating entry";
            return false;
        }
        auto* ehdr = reinterpret_cast<Elf64_Ehdr*>(processed_data.data());
        write_u64(reinterpret_cast<uint8_t*>(&ehdr->e_entry), new_entry, is_le);
    } else {
        if (new_entry > std::numeric_limits<uint32_t>::max()) {
            last_error = "32-bit entry value overflow";
            return false;
        }
        if (processed_data.size() < sizeof(Elf32_Ehdr)) {
            last_error = "ELF header out of bounds while updating entry";
            return false;
        }
        auto* ehdr = reinterpret_cast<Elf32_Ehdr*>(processed_data.data());
        write_u32(reinterpret_cast<uint8_t*>(&ehdr->e_entry), static_cast<uint32_t>(new_entry), is_le);
    }

    return true;
}

bool ElfSplitter::update_section_headers(const SegmentInfo& seg, const SegmentProcessResult& result) {
    if (result.inserted_bytes == 0 || parser.get_shoff() == 0 || parser.get_shnum() == 0) {
        return true;
    }

    const auto& sections = parser.get_sections();
    const bool is_le = parser.is_little_endian();
    const uint64_t seg_file_end = seg.p_offset + seg.p_filesz;
    const uint64_t seg_mem_end = seg.p_vaddr + seg.p_memsz;

    if (parser.is_64bit()) {
        for (size_t i = 0; i < sections.size(); ++i) {
            const uint64_t shoff = parser.get_shoff() + i * sizeof(Elf64_Shdr);
            if (shoff + sizeof(Elf64_Shdr) > processed_data.size()) {
                last_error = "Section header out of bounds while updating";
                return false;
            }

            const auto& sec = sections[i];
            uint64_t new_off = sec.sh_offset;
            uint64_t new_addr = sec.sh_addr;
            bool changed = false;

            if (sec.sh_offset >= seg.p_offset && sec.sh_offset < seg_file_end) {
                const uint64_t rel = sec.sh_offset - seg.p_offset;
                new_off = seg.p_offset + map_relative_offset(result, rel);
                changed = true;
            }

            if (sec.sh_addr >= seg.p_vaddr && sec.sh_addr < seg_mem_end) {
                const uint64_t rel = sec.sh_addr - seg.p_vaddr;
                new_addr = seg.p_vaddr + map_relative_offset(result, rel);
                changed = true;
            }

            if (changed) {
                auto* shdr = reinterpret_cast<Elf64_Shdr*>(processed_data.data() + shoff);
                write_u64(reinterpret_cast<uint8_t*>(&shdr->sh_offset), new_off, is_le);
                write_u64(reinterpret_cast<uint8_t*>(&shdr->sh_addr), new_addr, is_le);
            }
        }
    } else {
        for (size_t i = 0; i < sections.size(); ++i) {
            const uint64_t shoff = parser.get_shoff() + i * sizeof(Elf32_Shdr);
            if (shoff + sizeof(Elf32_Shdr) > processed_data.size()) {
                last_error = "Section header out of bounds while updating";
                return false;
            }

            const auto& sec = sections[i];
            uint64_t new_off = sec.sh_offset;
            uint64_t new_addr = sec.sh_addr;
            bool changed = false;

            if (sec.sh_offset >= seg.p_offset && sec.sh_offset < seg_file_end) {
                const uint64_t rel = sec.sh_offset - seg.p_offset;
                new_off = seg.p_offset + map_relative_offset(result, rel);
                changed = true;
            }

            if (sec.sh_addr >= seg.p_vaddr && sec.sh_addr < seg_mem_end) {
                const uint64_t rel = sec.sh_addr - seg.p_vaddr;
                new_addr = seg.p_vaddr + map_relative_offset(result, rel);
                changed = true;
            }

            if (changed) {
                if (new_off > std::numeric_limits<uint32_t>::max() ||
                    new_addr > std::numeric_limits<uint32_t>::max()) {
                    last_error = "32-bit section header value overflow";
                    return false;
                }
                auto* shdr = reinterpret_cast<Elf32_Shdr*>(processed_data.data() + shoff);
                write_u32(reinterpret_cast<uint8_t*>(&shdr->sh_offset), static_cast<uint32_t>(new_off), is_le);
                write_u32(reinterpret_cast<uint8_t*>(&shdr->sh_addr), static_cast<uint32_t>(new_addr), is_le);
            }
        }
    }

    return true;
}

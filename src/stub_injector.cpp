#include "stub_injector.h"
#include <cstring>
#include <algorithm>
#include <limits>

StubInjector::StubInjector(const ElfParser& parser, Architecture arch)
    : parser(parser), stub_gen(arch) {
    new_entry_point = parser.get_entry();
    stub_offset = 0;
    stub_size = 0;
}

StubInjector::~StubInjector() {}

bool StubInjector::inject_restore_stub(
    std::vector<uint8_t>& file_data,
    const std::vector<uint64_t>& insert_offsets,
    const std::vector<uint64_t>& insert_sizes,
    const StubInjectionOptions& options) {
    
    // 生成 stub
    StubInfo stub_info = stub_gen.generate_restore_stub(
        insert_offsets,
        insert_sizes,
        parser.get_entry()
    );
    
    if (stub_info.code.empty()) {
        log_error("Failed to generate stub");
        return false;
    }

    std::vector<uint8_t> payload = stub_info.code;
    payload.insert(payload.end(), stub_info.metadata.begin(), stub_info.metadata.end());

    uint64_t injection_point = file_data.size();
    if (options.has_fixed_stub_base) {
        injection_point = options.stub_base_offset;
        if (options.stub_region_size > 0 &&
            payload.size() > static_cast<size_t>(options.stub_region_size)) {
            log_error("Stub payload exceeds selected executable slack region");
            return false;
        }
        if (injection_point + payload.size() > file_data.size()) {
            log_error("Fixed stub location is out of file bounds");
            return false;
        }
    } else {
        const uint64_t required_size = injection_point + payload.size();
        if (required_size > file_data.size()) {
            file_data.resize(static_cast<size_t>(required_size), 0);
        }
    }
    
    // 注入 stub
    std::memcpy(file_data.data() + injection_point, 
                payload.data(), 
                payload.size());
    
    stub_offset = injection_point;
    stub_size = static_cast<uint64_t>(payload.size());
    
    log_info("Stub injected at offset 0x" + std::to_string(injection_point) +
             ", code=" + std::to_string(stub_info.code.size()) +
             ", metadata=" + std::to_string(stub_info.metadata.size()) +
             ", total=" + std::to_string(stub_size));

    if (options.update_entry_point) {
        if (!options.has_fixed_stub_base || options.stub_base_vaddr == 0) {
            log_error("stub-update-entry requires fixed executable stub placement");
            return false;
        }

        if (parser.get_machine() == EM_X86_64 && stub_info.code.size() >= 5) {
            const int64_t rel64 =
                static_cast<int64_t>(stub_info.original_entry) -
                static_cast<int64_t>(options.stub_base_vaddr + 5);
            if (rel64 < std::numeric_limits<int32_t>::min() ||
                rel64 > std::numeric_limits<int32_t>::max()) {
                log_error("x86_64 entry-jump target is out of rel32 range");
                return false;
            }
            uint8_t* stub_ptr = file_data.data() + injection_point;
            const int32_t rel32 = static_cast<int32_t>(rel64);
            stub_ptr[0] = 0xE9;  // jmp rel32
            std::memcpy(stub_ptr + 1, &rel32, sizeof(rel32));
            // 剩余字节保留，不影响执行；入口会在 5 字节后直接跳转离开。
        }

        new_entry_point = options.stub_base_vaddr;
        if (!update_elf_header(file_data, new_entry_point)) {
            log_error("Failed to update ELF header for stub entry");
            return false;
        }
        log_info("Entry point updated to 0x" + std::to_string(new_entry_point));
    }
    
    return true;
}

bool StubInjector::inject_custom_stub(
    std::vector<uint8_t>& file_data,
    const std::vector<uint8_t>& stub_code,
    const StubInjectionOptions& options) {
    
    StubInfo stub_info = stub_gen.generate_custom_stub(
        stub_code,
        parser.get_entry()
    );
    
    uint64_t injection_point = file_data.size();
    if (options.has_fixed_stub_base) {
        injection_point = options.stub_base_offset;
        if (options.stub_region_size > 0 &&
            stub_info.code.size() > static_cast<size_t>(options.stub_region_size)) {
            log_error("Custom stub exceeds selected executable slack region");
            return false;
        }
        if (injection_point + stub_info.code.size() > file_data.size()) {
            log_error("Fixed custom-stub location is out of file bounds");
            return false;
        }
    } else {
        const uint64_t required_size = injection_point + stub_info.code.size();
        if (required_size > file_data.size()) {
            file_data.resize(static_cast<size_t>(required_size), 0);
        }
    }
    
    std::memcpy(file_data.data() + injection_point,
                stub_info.code.data(),
                stub_info.code.size());
    
    stub_offset = injection_point;
    stub_size = stub_info.size;

    if (options.update_entry_point) {
        if (!options.has_fixed_stub_base || options.stub_base_vaddr == 0) {
            log_error("stub-update-entry requires fixed executable stub placement");
            return false;
        }
        new_entry_point = options.stub_base_vaddr;
        if (!update_elf_header(file_data, new_entry_point)) {
            log_error("Failed to update ELF header for custom stub entry");
            return false;
        }
        log_info("Entry point updated to 0x" + std::to_string(new_entry_point));
    }
    return true;
}

bool StubInjector::inject_decrypt_stub(
    std::vector<uint8_t>& file_data,
    uint64_t code_start,
    uint64_t code_size,
    const uint8_t* key,
    size_t key_size,
    const StubInjectionOptions& options) {
    (void)file_data;
    (void)code_start;
    (void)code_size;
    (void)key;
    (void)key_size;
    (void)options;
    
    // TODO: 实现解密 stub 注入
    return true;
}

bool StubInjector::find_stub_injection_point(
    uint64_t& injection_point,
    uint64_t required_size) {
    
    std::vector<SegmentInfo> segments;
    for (const auto& seg : parser.get_segments()) {
        if (seg.p_filesz == 0) {
            continue;
        }
        segments.push_back(seg);
    }
    std::sort(segments.begin(), segments.end(),
              [](const SegmentInfo& lhs, const SegmentInfo& rhs) {
                  return lhs.p_offset < rhs.p_offset;
              });
    
    // 策略：在第一个 PT_LOAD 段之后、第二个 PT_LOAD 段之前找空隙
    if (segments.size() < 2) {
        injection_point = parser.get_file_data().size();
        return true;
    }

    for (size_t i = 0; i + 1 < segments.size(); ++i) {
        const auto& seg1 = segments[i];
        const auto& seg2 = segments[i + 1];
        
        uint64_t gap_start = seg1.get_end_offset();
        if (gap_start < sizeof(Elf64_Ehdr)) {
            gap_start = sizeof(Elf64_Ehdr);
        }
        if (seg2.p_offset <= gap_start) {
            continue;
        }
        uint64_t gap_size = seg2.p_offset - gap_start;
        
        if (gap_size >= required_size) {
            injection_point = gap_start;
            return true;
        }
    }
    
    // 备选：在文件末尾
    injection_point = parser.get_file_data().size();
    return true;
}

bool StubInjector::update_elf_header(
    std::vector<uint8_t>& file_data,
    uint64_t new_entry) {
    
    bool is_le = parser.is_little_endian();
    
    if (parser.is_64bit()) {
        if (file_data.size() < sizeof(Elf64_Ehdr)) {
            return false;
        }
        Elf64_Ehdr* hdr = (Elf64_Ehdr*)file_data.data();
        write_u64((uint8_t*)&hdr->e_entry, new_entry, is_le);
    } else {
        if (file_data.size() < sizeof(Elf32_Ehdr)) {
            return false;
        }
        Elf32_Ehdr* hdr = (Elf32_Ehdr*)file_data.data();
        write_u32((uint8_t*)&hdr->e_entry, new_entry, is_le);
    }
    
    return true;
}

bool StubInjector::update_program_headers(
    std::vector<uint8_t>& file_data,
    uint64_t stub_offset,
    uint64_t stub_size,
    uint64_t new_entry) {
    (void)file_data;
    (void)stub_offset;
    (void)stub_size;
    (void)new_entry;
    
    // TODO: 更新程序头以包含 stub 段
    return true;
}

bool StubInjector::create_stub_segment(
    std::vector<uint8_t>& file_data,
    uint64_t stub_offset,
    uint64_t stub_size) {
    (void)file_data;
    (void)stub_offset;
    (void)stub_size;
    
    // TODO: 创建新的 PT_LOAD 段来容纳 stub
    return true;
}

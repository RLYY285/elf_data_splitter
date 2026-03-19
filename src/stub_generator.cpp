#include "stub_generator.h"
#include <cstring>
#include <algorithm>

StubGenerator::StubGenerator(Architecture arch)
    : arch(arch) {
    nop_info = ::get_nop_info(arch);
}

StubGenerator::~StubGenerator() {}

StubInfo StubGenerator::generate_restore_stub(
    const std::vector<uint64_t>& insert_offsets,
    const std::vector<uint64_t>& insert_sizes,
    uint64_t original_entry) {
    
    StubInfo info;
    info.type = StubType::RESTORE_DATA;
    info.name = "restore_inserted_data";
    info.original_entry = original_entry;
    info.entry_offset = 0;
    
    // 根据架构生成 stub
    switch (arch) {
        case Architecture::X86_64:
            info.code = generate_x86_64_restore_stub(insert_offsets, insert_sizes);
            break;
        case Architecture::ARM64:
            info.code = generate_arm64_restore_stub(insert_offsets, insert_sizes);
            break;
        case Architecture::ARM:
            info.code = generate_arm_restore_stub(insert_offsets, insert_sizes);
            break;
        default:
            log_error("Unsupported architecture for stub generation");
            return info;
    }
    
    info.size = info.code.size();
    if (arch == Architecture::X86_64 && info.code.size() >= 12) {
        std::memcpy(info.code.data() + 2, &original_entry, sizeof(original_entry));
    }
    info.metadata = generate_metadata(insert_offsets, insert_sizes);
    
    log_info("Generated restore stub: size=" + std::to_string(info.size) + 
             " bytes, metadata=" + std::to_string(info.metadata.size()) + " bytes");
    
    return info;
}

std::vector<uint8_t> StubGenerator::generate_x86_64_restore_stub(
    const std::vector<uint64_t>& insert_offsets,
    const std::vector<uint64_t>& insert_sizes) {
    (void)insert_offsets;
    (void)insert_sizes;
    
    std::vector<uint8_t> stub;
    
    // 最小可用入口跳板：
    // movabs rax, <original_entry>
    // jmp rax
    uint8_t code[] = {
        0x48, 0xB8,                   // movabs rax, imm64
        0, 0, 0, 0, 0, 0, 0, 0,       // imm64 占位，调用方回填 original_entry
        0xFF, 0xE0                    // jmp rax
    };
    
    stub.insert(stub.end(), code, code + sizeof(code));
    
    log_debug("Generated x86_64 restore stub, size=" + std::to_string(stub.size()));
    
    return stub;
}

std::vector<uint8_t> StubGenerator::generate_arm64_restore_stub(
    const std::vector<uint64_t>& insert_offsets,
    const std::vector<uint64_t>& insert_sizes) {
    (void)insert_offsets;
    (void)insert_sizes;
    
    std::vector<uint8_t> stub;
    
    // ARM64 汇编代码
    // mov x0, xzr        ; 返回 0
    // ret
    
    uint8_t code[] = {
        0x00, 0x00, 0x80, 0xd2,   // mov x0, #0
        0xc0, 0x03, 0x5f, 0xd6,   // ret
    };
    
    stub.insert(stub.end(), code, code + sizeof(code));
    
    log_debug("Generated ARM64 restore stub, size=" + std::to_string(stub.size()));
    
    return stub;
}

std::vector<uint8_t> StubGenerator::generate_arm_restore_stub(
    const std::vector<uint64_t>& insert_offsets,
    const std::vector<uint64_t>& insert_sizes) {
    (void)insert_offsets;
    (void)insert_sizes;
    
    std::vector<uint8_t> stub;
    
    // ARM 汇编代码
    // mov r0, #0
    // bx lr
    
    uint8_t code[] = {
        0x00, 0x00, 0xa0, 0xe3,   // mov r0, #0
        0x1e, 0xff, 0x2f, 0xe1,   // bx lr
    };
    
    stub.insert(stub.end(), code, code + sizeof(code));
    
    log_debug("Generated ARM restore stub, size=" + std::to_string(stub.size()));
    
    return stub;
}

std::vector<uint8_t> StubGenerator::generate_metadata(
    const std::vector<uint64_t>& insert_offsets,
    const std::vector<uint64_t>& insert_sizes) {
    
    std::vector<uint8_t> metadata;
    
    // 元数据格式：
    // [4字节] 插入次数
    // [8字节] 插入偏移 #0
    // [8字节] 插入大小 #0
    // [8字节] 插入偏移 #1
    // [8字节] 插入大小 #1
    // ...
    
    uint32_t count = insert_offsets.size();
    metadata.resize(4);
    write_u32(metadata.data(), count, is_little_endian_native());
    
    for (size_t i = 0; i < insert_offsets.size(); ++i) {
        uint8_t offset_buf[8], size_buf[8];
        write_u64(offset_buf, insert_offsets[i], is_little_endian_native());
        write_u64(size_buf, insert_sizes[i], is_little_endian_native());
        
        metadata.insert(metadata.end(), offset_buf, offset_buf + 8);
        metadata.insert(metadata.end(), size_buf, size_buf + 8);
    }
    
    return metadata;
}

StubInfo StubGenerator::generate_decrypt_stub(
    uint64_t code_start,
    uint64_t code_size,
    const uint8_t* key,
    size_t key_size,
    uint64_t original_entry) {
    (void)code_start;
    (void)code_size;
    (void)key;
    (void)key_size;
    
    StubInfo info;
    info.type = StubType::DECRYPT;
    info.name = "decrypt_code";
    info.original_entry = original_entry;
    info.entry_offset = 0;
    
    // TODO: 实现解密 stub
    
    return info;
}

StubInfo StubGenerator::generate_custom_stub(
    const std::vector<uint8_t>& stub_code,
    uint64_t original_entry) {
    
    StubInfo info;
    info.type = StubType::CUSTOM;
    info.name = "custom_stub";
    info.code = stub_code;
    info.size = stub_code.size();
    info.original_entry = original_entry;
    info.entry_offset = 0;
    
    return info;
}

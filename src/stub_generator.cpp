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

StubInfo StubGenerator::generate_restore_stub(
    const std::vector<uint64_t>& insert_vaddrs,
    const std::vector<uint64_t>& insert_sizes,
    const std::vector<uint64_t>& move_lens,
    uint64_t stub_vaddr,
    uint64_t original_entry) {

    StubInfo info;
    info.type = StubType::RESTORE_DATA;
    info.name = "restore_inserted_data_v2";
    info.original_entry = original_entry;
    info.entry_offset = 0;

    if (arch != Architecture::X86_64) {
        log_error("Full restore stub (v2) only supports x86_64");
        return info;
    }

    info.code = generate_x86_64_restore_stub_v2();
    if (info.code.empty()) {
        return info;
    }

    // Patch the LEA disp32 at byte offset 14 with the actual code size so that
    // r12 = abs(stub_start) + CODE_SIZE = start of metadata at runtime.
    const uint32_t code_size = static_cast<uint32_t>(info.code.size());
    info.code[14] = static_cast<uint8_t>((code_size >>  0) & 0xFF);
    info.code[15] = static_cast<uint8_t>((code_size >>  8) & 0xFF);
    info.code[16] = static_cast<uint8_t>((code_size >> 16) & 0xFF);
    info.code[17] = static_cast<uint8_t>((code_size >> 24) & 0xFF);

    info.size = info.code.size();
    info.metadata = generate_metadata_v2(
        insert_vaddrs, insert_sizes, move_lens, stub_vaddr, original_entry);

    log_info("Generated x86_64 restore stub v2: code=" + std::to_string(info.size) +
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

std::vector<uint8_t> StubGenerator::generate_metadata_v2(
    const std::vector<uint64_t>& insert_vaddrs,
    const std::vector<uint64_t>& insert_sizes,
    const std::vector<uint64_t>& move_lens,
    uint64_t stub_vaddr,
    uint64_t original_entry) {

    // New metadata format (little-endian x86_64 target):
    //   [0..7]   entry_delta  (int64_t): original_entry - stub_vaddr
    //   [8..11]  count        (uint32_t): number of records
    //   For each record (24 bytes each):
    //     [0..7]   vaddr_delta  (int64_t): insert_vaddrs[i] - stub_vaddr
    //     [8..15]  insert_size  (uint64_t)
    //     [16..23] move_len     (uint64_t)
    std::vector<uint8_t> md;
    md.reserve(12 + insert_vaddrs.size() * 24);

    uint8_t buf[8];
    const int64_t entry_delta =
        static_cast<int64_t>(original_entry) - static_cast<int64_t>(stub_vaddr);
    write_u64(buf, static_cast<uint64_t>(entry_delta), true /* LE */);
    md.insert(md.end(), buf, buf + 8);

    const uint32_t count = static_cast<uint32_t>(insert_vaddrs.size());
    write_u32(buf, count, true /* LE */);
    md.insert(md.end(), buf, buf + 4);

    for (size_t i = 0; i < insert_vaddrs.size(); ++i) {
        const int64_t vaddr_delta =
            static_cast<int64_t>(insert_vaddrs[i]) - static_cast<int64_t>(stub_vaddr);
        write_u64(buf, static_cast<uint64_t>(vaddr_delta), true /* LE */);
        md.insert(md.end(), buf, buf + 8);

        write_u64(buf, insert_sizes[i], true /* LE */);
        md.insert(md.end(), buf, buf + 8);

        const uint64_t ml = (i < move_lens.size()) ? move_lens[i] : 0ULL;
        write_u64(buf, ml, true /* LE */);
        md.insert(md.end(), buf, buf + 8);
    }

    return md;
}

std::vector<uint8_t> StubGenerator::generate_x86_64_restore_stub_v2() {
    // x86_64 position-independent restore stub (157 bytes).
    //
    // Algorithm (no cumulative-removed tracking required):
    //   Key insight: after k-1 prior removals within any segment, the virtual
    //   address of insertion k is exactly  stub_vaddr + vaddr_delta[k], so
    //   each memmove can use that address directly without adjustment.
    //
    // Register usage:
    //   r15 = abs(stub_start)           r14 = abs(original_entry)
    //   r13 = remaining count           r12 = current metadata record pointer
    //   rax = current insertion vaddr   r8  = insert_size   r9 = move_len
    //
    // Metadata layout (immediately after code, pointed to by the patched LEA):
    //   [0..7]   entry_delta  (int64_t): original_entry - stub_vaddr
    //   [8..11]  count        (uint32_t)
    //   Records at [12..] (24 bytes each):
    //     [0..7]   vaddr_delta  (int64_t): seg.p_vaddr + insertion_points[k] - stub_vaddr
    //     [8..15]  insert_size  (uint64_t)
    //     [16..23] move_len     (uint64_t)
    //
    // Bytes 14-17 (LEA disp32) are patched by the caller with the code size (157)
    // so that  r12 = abs(stub_start) + code_size  points at the metadata.
    static const uint8_t template_code[] = {
        // +0    call .get_rip
        0xe8, 0x00, 0x00, 0x00, 0x00,
        // +5    pop r15
        0x41, 0x5f,
        // +7    sub r15, 5   (r15 = abs stub_start)
        0x49, 0x83, 0xef, 0x05,
        // +11   lea r12, [r15 + disp32]  — disp32 at bytes [14..17] patched to CODE_SIZE
        0x4d, 0x8d, 0xa7,
        0x7f, 0x7f, 0x7f, 0x7f,              // placeholder (+14)
        // +18   mov r14, [r12]               — entry_delta
        0x4d, 0x8b, 0x34, 0x24,
        // +22   add r14, r15                 — abs original entry
        0x4d, 0x01, 0xfe,
        // +25   mov r13d, dword [r12+8]      — count
        0x45, 0x8b, 0x6c, 0x24, 0x08,
        // +30   add r12, 12                  — advance to first record
        0x49, 0x83, 0xc4, 0x0c,
        // .loop (+34):
        // +34   test r13d, r13d
        0x45, 0x85, 0xed,
        // +37   jz .done  (rel8 = +0x73 = 115, target = 154)
        0x74, 0x73,
        // +39   dec r13d
        0x41, 0xff, 0xcd,
        // +42   mov rax, [r12]               — vaddr_delta
        0x49, 0x8b, 0x04, 0x24,
        // +46   add rax, r15                 — abs insertion vaddr (already correct)
        0x4c, 0x01, 0xf8,
        // +49   mov r8, [r12+8]              — insert_size
        0x4d, 0x8b, 0x44, 0x24, 0x08,
        // +54   mov r9, [r12+16]             — move_len
        0x4d, 0x8b, 0x4c, 0x24, 0x10,
        // +59   add r12, 24                  — advance to next record
        0x49, 0x83, 0xc4, 0x18,
        // Save registers before mprotect syscall
        // +63   push rax
        0x50,
        // +64   push r8
        0x41, 0x50,
        // +66   push r9
        0x41, 0x51,
        // +68   push r12
        0x41, 0x54,
        // +70   push r13
        0x41, 0x55,
        // +72   push r14
        0x41, 0x56,
        // +74   push r15
        0x41, 0x57,
        // mprotect(page_floor(rax), page_ceil(r8+r9)+4096, PROT_RWX)
        // +76   mov rdi, rax
        0x48, 0x89, 0xc7,
        // +79   and rdi, -4096
        0x48, 0x81, 0xe7, 0x00, 0xf0, 0xff, 0xff,
        // +86   mov rsi, r8
        0x4c, 0x89, 0xc6,
        // +89   add rsi, r9
        0x4c, 0x01, 0xce,
        // +92   add rsi, 4095
        0x48, 0x81, 0xc6, 0xff, 0x0f, 0x00, 0x00,
        // +99   and rsi, -4096
        0x48, 0x81, 0xe6, 0x00, 0xf0, 0xff, 0xff,
        // +106  add rsi, 4096
        0x48, 0x81, 0xc6, 0x00, 0x10, 0x00, 0x00,
        // +113  mov edx, 7
        0xba, 0x07, 0x00, 0x00, 0x00,
        // +118  mov eax, 10   (SYS_mprotect)
        0xb8, 0x0a, 0x00, 0x00, 0x00,
        // +123  syscall
        0x0f, 0x05,
        // Pop in reverse order
        // +125  pop r15
        0x41, 0x5f,
        // +127  pop r14
        0x41, 0x5e,
        // +129  pop r13
        0x41, 0x5d,
        // +131  pop r12
        0x41, 0x5c,
        // +133  pop r9
        0x41, 0x59,
        // +135  pop r8
        0x41, 0x58,
        // +137  pop rax
        0x58,
        // memmove via rep movsb (src > dst, forward copy is safe)
        // +138  mov rdi, rax
        0x48, 0x89, 0xc7,
        // +141  mov rsi, rax
        0x48, 0x89, 0xc6,
        // +144  add rsi, r8               (src = insertion_end)
        0x4c, 0x01, 0xc6,
        // +147  mov rcx, r9
        0x4c, 0x89, 0xc9,
        // +150  rep movsb
        0xf3, 0xa4,
        // +152  jmp .loop  (rel8 = -120 = 0x88, target = 34)
        0xeb, 0x88,
        // .done (+154):
        // +154  jmp r14
        0x41, 0xff, 0xe6,
    };

    std::vector<uint8_t> stub(template_code, template_code + sizeof(template_code));
    return stub;
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

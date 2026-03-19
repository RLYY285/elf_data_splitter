#include "stub_generator.h"
#include <cstring>
#include <algorithm>

StubGenerator::StubGenerator(Architecture arch)
    : arch(arch) {
    nop_info = ::get_nop_info(arch);
}

StubGenerator::~StubGenerator() {}

StubInfo StubGenerator::generate_restore_stub(
    const std::vector<uint64_t>& insert_vaddrs,
    const std::vector<uint64_t>& insert_sizes,
    const std::vector<uint64_t>& bytes_to_move,
    uint64_t original_entry) {
    
    StubInfo info;
    info.type = StubType::RESTORE_DATA;
    info.name = "restore_inserted_data";
    info.original_entry = original_entry;
    info.entry_offset = 0;
    
    // 根据架构生成 stub
    switch (arch) {
        case Architecture::X86_64:
            info.code = generate_x86_64_restore_stub(insert_vaddrs, insert_sizes, bytes_to_move);
            break;
        case Architecture::ARM64:
            info.code = generate_arm64_restore_stub(insert_vaddrs, insert_sizes, bytes_to_move);
            break;
        case Architecture::ARM:
            info.code = generate_arm_restore_stub(insert_vaddrs, insert_sizes, bytes_to_move);
            break;
        default:
            log_error("Unsupported architecture for stub generation");
            return info;
    }
    
    info.size = info.code.size();
    info.metadata = generate_metadata(insert_vaddrs, insert_sizes, bytes_to_move, original_entry);
    
    log_info("Generated restore stub: size=" + std::to_string(info.size) + 
             " bytes, metadata=" + std::to_string(info.metadata.size()) + " bytes");
    
    return info;
}

std::vector<uint8_t> StubGenerator::generate_x86_64_restore_stub(
    const std::vector<uint64_t>& insert_vaddrs,
    const std::vector<uint64_t>& insert_sizes,
    const std::vector<uint64_t>& bytes_to_move) {
    (void)insert_vaddrs;
    (void)insert_sizes;
    (void)bytes_to_move;
    
    // x86_64 运行时恢复 stub（位置无关，86 字节）
    //
    // 布局：[stub_code(86字节)][metadata]
    // metadata 格式：
    //   [4字节] count               插入块数量
    //   [count * 24字节] entries    每条: (vaddr:8, insert_size:8, bytes_to_move:8)
    //   [8字节] original_entry      真实入口点
    //
    // 算法（逆序处理，从最后一个插入块到第一个）：
    //   for i = count-1 downto 0:
    //     memmove(entries[i].vaddr,
    //             entries[i].vaddr + entries[i].insert_size,
    //             entries[i].bytes_to_move)
    //   jmp original_entry
    //
    // 汇编（AT&T 语法已用 gcc 验证）：
    //   lea    0x4f(%rip),%rbx        // rbx = metadata (rip+7+0x4f = rip+86)
    //   mov    (%rbx),%ecx            // ecx = count
    //   test   %ecx,%ecx
    //   je     done
    //   mov    %ecx,%r8d              // r8 = count * 24
    //   imul   $0x18,%r8,%r8
    //   add    %rbx,%r8
    //   add    $0x4,%r8               // r8 = &original_entry (= metadata+4+count*24)
    //   lea    0x4(%rbx),%rsi         // rsi = &entries[0]
    //   mov    %rcx,%rax
    //   dec    %rax
    //   imul   $0x18,%rax,%rax
    //   add    %rax,%rsi              // rsi = &entries[count-1]
    // loop_start:
    //   mov    (%rsi),%rdi            // rdi = vaddr (dst)
    //   mov    0x8(%rsi),%r9          // r9 = insert_size
    //   mov    0x10(%rsi),%rdx        // rdx = bytes_to_move
    //   sub    $0x18,%rsi             // move to previous entry
    //   push   %rsi
    //   push   %rcx
    //   push   %r8
    //   lea    (%rdi,%r9,1),%rsi      // rsi = vaddr + insert_size (src)
    //   mov    %rdx,%rcx
    //   cld
    //   rep movsb                     // forward copy: [rdi..(rdi+rcx)] <- [rsi..(rsi+rcx)]
    //   pop    %r8
    //   pop    %rcx
    //   pop    %rsi
    //   dec    %ecx
    //   jne    loop_start
    // done:
    //   mov    (%r8),%rax             // rax = original_entry
    //   jmp    *%rax

    static const uint8_t CODE[] = {
        // 0x00: lea 0x4f(%rip),%rbx  (displacement 0x4f = 79 = 86 - 7)
        0x48, 0x8D, 0x1D, 0x4F, 0x00, 0x00, 0x00,
        // 0x07: mov (%rbx),%ecx
        0x8B, 0x0B,
        // 0x09: test %ecx,%ecx
        0x85, 0xC9,
        // 0x0B: je done (+0x44)
        0x74, 0x44,
        // 0x0D: mov %ecx,%r8d
        0x41, 0x89, 0xC8,
        // 0x10: imul $0x18,%r8,%r8
        0x4D, 0x6B, 0xC0, 0x18,
        // 0x14: add %rbx,%r8
        0x49, 0x01, 0xD8,
        // 0x17: add $0x4,%r8
        0x49, 0x83, 0xC0, 0x04,
        // 0x1B: lea 0x4(%rbx),%rsi
        0x48, 0x8D, 0x73, 0x04,
        // 0x1F: mov %rcx,%rax
        0x48, 0x89, 0xC8,
        // 0x22: dec %rax
        0x48, 0xFF, 0xC8,
        // 0x25: imul $0x18,%rax,%rax
        0x48, 0x6B, 0xC0, 0x18,
        // 0x29: add %rax,%rsi
        0x48, 0x01, 0xC6,
        // 0x2C (loop_start): mov (%rsi),%rdi
        0x48, 0x8B, 0x3E,
        // 0x2F: mov 0x8(%rsi),%r9
        0x4C, 0x8B, 0x4E, 0x08,
        // 0x33: mov 0x10(%rsi),%rdx
        0x48, 0x8B, 0x56, 0x10,
        // 0x37: sub $0x18,%rsi
        0x48, 0x83, 0xEE, 0x18,
        // 0x3B: push %rsi
        0x56,
        // 0x3C: push %rcx
        0x51,
        // 0x3D: push %r8
        0x41, 0x50,
        // 0x3F: lea (%rdi,%r9,1),%rsi
        0x4A, 0x8D, 0x34, 0x0F,
        // 0x43: mov %rdx,%rcx
        0x48, 0x89, 0xD1,
        // 0x46: cld
        0xFC,
        // 0x47: rep movsb
        0xF3, 0xA4,
        // 0x49: pop %r8
        0x41, 0x58,
        // 0x4B: pop %rcx
        0x59,
        // 0x4C: pop %rsi
        0x5E,
        // 0x4D: dec %ecx
        0xFF, 0xC9,
        // 0x4F: jne loop_start (-0x25 signed = 0xDB)
        0x75, 0xDB,
        // 0x51 (done): mov (%r8),%rax
        0x49, 0x8B, 0x00,
        // 0x54: jmp *%rax
        0xFF, 0xE0,
        // Total: 86 bytes (0x56)
    };
    
    std::vector<uint8_t> stub(CODE, CODE + sizeof(CODE));
    
    log_debug("Generated x86_64 restore stub, code_size=" + std::to_string(stub.size()));
    
    return stub;
}

std::vector<uint8_t> StubGenerator::generate_arm64_restore_stub(
    const std::vector<uint64_t>& insert_vaddrs,
    const std::vector<uint64_t>& insert_sizes,
    const std::vector<uint64_t>& bytes_to_move) {
    (void)insert_vaddrs;
    (void)insert_sizes;
    (void)bytes_to_move;
    
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
    const std::vector<uint64_t>& insert_vaddrs,
    const std::vector<uint64_t>& insert_sizes,
    const std::vector<uint64_t>& bytes_to_move) {
    (void)insert_vaddrs;
    (void)insert_sizes;
    (void)bytes_to_move;
    
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
    const std::vector<uint64_t>& insert_vaddrs,
    const std::vector<uint64_t>& insert_sizes,
    const std::vector<uint64_t>& bytes_to_move,
    uint64_t original_entry) {
    
    std::vector<uint8_t> metadata;
    bool is_le = is_little_endian_native();
    
    // 元数据格式：
    // [4字节]        count           插入块数量
    // [count*24字节] entries         每条：(vaddr:8, insert_size:8, bytes_to_move:8)
    // [8字节]        original_entry  原始入口点
    
    uint32_t count = static_cast<uint32_t>(insert_vaddrs.size());
    metadata.resize(4);
    write_u32(metadata.data(), count, is_le);
    
    for (size_t i = 0; i < insert_vaddrs.size(); ++i) {
        uint8_t buf[8];
        write_u64(buf, insert_vaddrs[i], is_le);
        metadata.insert(metadata.end(), buf, buf + 8);
        write_u64(buf, insert_sizes[i], is_le);
        metadata.insert(metadata.end(), buf, buf + 8);
        write_u64(buf, bytes_to_move[i], is_le);
        metadata.insert(metadata.end(), buf, buf + 8);
    }
    
    uint8_t entry_buf[8];
    write_u64(entry_buf, original_entry, is_le);
    metadata.insert(metadata.end(), entry_buf, entry_buf + 8);
    
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

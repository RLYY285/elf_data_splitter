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
    uint64_t original_entry,
    const std::vector<SegmentInfo>& segments) {
    
    StubInfo info;
    info.type = StubType::RESTORE_DATA;
    info.name = "restore_inserted_data";
    info.original_entry = original_entry;
    info.entry_offset = 0;
    
    // 根据架构生成 stub
    switch (arch) {
        case Architecture::X86_64:
            info.code = generate_x86_64_restore_stub();
            break;
        case Architecture::ARM64:
            info.code = generate_arm64_restore_stub();
            break;
        case Architecture::ARM:
            info.code = generate_arm_restore_stub();
            break;
        default:
            log_error("Unsupported architecture for stub generation");
            return info;
    }
    
    info.size = info.code.size();
    info.metadata = generate_metadata(insert_offsets, insert_sizes, original_entry, segments);
    
    log_info("Generated restore stub: size=" + std::to_string(info.size) + 
             " bytes, metadata=" + std::to_string(info.metadata.size()) + " bytes");
    
    return info;
}

// 121 字节 x86_64 恢复 stub
// 布局（紧跟 stub 代码之后）：
//   [metadata: ELFS magic + version + original_entry + count + repair_ops...]
//
// Stub 逻辑：
//   1. 保存所有通用寄存器 + rflags
//   2. lea rbx, [rip+0x5F]  →  指向 metadata 首字节（stub 结束后 = 偏移 121）
//   3. 检查 magic "ELFS"
//   4. 从 metadata+5 读取 OriginalEntry，写入栈上保存的 rax 槽（rsp+0x60=96）
//   5. 从 metadata+13 读取 repair op 数量
//   6. 从最后一个 op 往前逐一执行 memmove(dst, src, count) 删除插入数据
//   7. popfq + 弹出所有寄存器（rax = OriginalEntry）
//   8. jmp rax
std::vector<uint8_t> StubGenerator::generate_x86_64_restore_stub() {
    // 偏移注释：[byte_offset] instruction (size)
    static const uint8_t k_stub[] = {
        // ---- 保存寄存器 ----
        0x50,                               // [0]   push rax
        0x53,                               // [1]   push rbx
        0x51,                               // [2]   push rcx
        0x52,                               // [3]   push rdx
        0x56,                               // [4]   push rsi
        0x57,                               // [5]   push rdi
        0x41, 0x50,                         // [6]   push r8
        0x41, 0x51,                         // [8]   push r9
        0x41, 0x52,                         // [10]  push r10
        0x41, 0x53,                         // [12]  push r11
        0x41, 0x54,                         // [14]  push r12
        0x41, 0x55,                         // [16]  push r13
        0x9C,                               // [18]  pushfq
        // ---- 定位 metadata（紧跟 stub 后，偏移 121）----
        // lea rbx, [rip+0x5F]  ;  RIP=[26], metadata=[121], disp=121-26=95=0x5F
        0x48, 0x8D, 0x1D, 0x5F, 0x00, 0x00, 0x00, // [19] lea rbx, [rip+0x5F]
        // ---- 检查 magic "ELFS" (LE uint32 = 0x53464C45) ----
        // cmp dword [rbx], 0x53464C45
        0x81, 0x3B, 0x45, 0x4C, 0x46, 0x53,       // [26] cmp [rbx], 0x53464C45
        0x75, 0x42,                         // [32]  jne .done  (+66 → [100])
        // ---- 将 OriginalEntry 写入栈上保存的 rax（rsp+96=0x60） ----
        0x48, 0x8B, 0x43, 0x05,             // [34]  mov rax, [rbx+5]
        0x48, 0x89, 0x44, 0x24, 0x60,       // [38]  mov [rsp+0x60], rax
        // ---- 读取 repair op 数量（uint16 at metadata+13） ----
        0x4C, 0x0F, 0xB7, 0x4B, 0x0D,       // [43]  movzx r9, word [rbx+13]
        0x4D, 0x85, 0xC9,                   // [48]  test r9, r9
        0x74, 0x2F,                         // [51]  jz .done   (+47 → [100])
        // ---- 计算最后一个 repair op 的指针 ----
        // lea r11, [rbx+15]   ; 第一个 op
        0x4C, 0x8D, 0x5B, 0x0F,             // [53]  lea r11, [rbx+15]
        // imul r12, r9, 24    ; r12 = count*24（每个 op 为 3×uint64=24 字节）
        0x4D, 0x6B, 0xE1, 0x18,             // [57]  imul r12, r9, 24
        // add r12, r11        ; r12 = 超出最后一个 op 的指针
        0x4D, 0x01, 0xDC,                   // [61]  add r12, r11
        // sub r12, 24         ; r12 = 指向最后一个 op
        0x49, 0x83, 0xEC, 0x18,             // [64]  sub r12, 24
        // ---- ins_loop: 从后往前执行 memmove ----
        // .ins_loop: [68]
        0x49, 0x8B, 0x7C, 0x24, 0x00,       // [68]  mov rdi, [r12+0]    ; dst
        0x49, 0x8B, 0x74, 0x24, 0x08,       // [73]  mov rsi, [r12+8]    ; src
        0x49, 0x8B, 0x4C, 0x24, 0x10,       // [78]  mov rcx, [r12+16]   ; count
        0x48, 0x85, 0xC9,                   // [83]  test rcx, rcx
        0x74, 0x03,                         // [86]  jz .skip   (+3 → [91])
        0xFC,                               // [88]  cld
        0xF3, 0xA4,                         // [89]  rep movsb
        // .skip: [91]
        0x49, 0x83, 0xEC, 0x18,             // [91]  sub r12, 24
        0x49, 0xFF, 0xC9,                   // [95]  dec r9
        0x75, 0xE0,                         // [98]  jnz .ins_loop  (-32 → [68])
        // ---- .done: 恢复寄存器并跳转 ----
        // .done: [100]
        0x9D,                               // [100] popfq
        0x41, 0x5D,                         // [101] pop r13
        0x41, 0x5C,                         // [103] pop r12
        0x41, 0x5B,                         // [105] pop r11
        0x41, 0x5A,                         // [107] pop r10
        0x41, 0x59,                         // [109] pop r9
        0x41, 0x58,                         // [111] pop r8
        0x5F,                               // [113] pop rdi
        0x5E,                               // [114] pop rsi
        0x5A,                               // [115] pop rdx
        0x59,                               // [116] pop rcx
        0x5B,                               // [117] pop rbx
        0x58,                               // [118] pop rax   ← OriginalEntry（由上方写入）
        0xFF, 0xE0,                         // [119] jmp rax
        // Total: 121 bytes
    };
    
    std::vector<uint8_t> stub(k_stub, k_stub + sizeof(k_stub));
    log_debug("Generated x86_64 restore stub, size=" + std::to_string(stub.size()));
    return stub;
}

std::vector<uint8_t> StubGenerator::generate_arm64_restore_stub() {
    // ARM64: 无完整恢复实现，生成最简跳板（调用方负责设置入口）
    uint8_t code[] = {
        0x00, 0x00, 0x80, 0xd2,   // mov x0, #0
        0xc0, 0x03, 0x5f, 0xd6,   // ret
    };
    std::vector<uint8_t> stub(code, code + sizeof(code));
    log_debug("Generated ARM64 restore stub, size=" + std::to_string(stub.size()));
    return stub;
}

std::vector<uint8_t> StubGenerator::generate_arm_restore_stub() {
    // ARM: 无完整恢复实现，生成最简跳板
    uint8_t code[] = {
        0x00, 0x00, 0xa0, 0xe3,   // mov r0, #0
        0x1e, 0xff, 0x2f, 0xe1,   // bx lr
    };
    std::vector<uint8_t> stub(code, code + sizeof(code));
    log_debug("Generated ARM restore stub, size=" + std::to_string(stub.size()));
    return stub;
}

// 元数据格式：
// [0-3]:   'E','L','F','S'  magic
// [4]:     0x01             version
// [5-12]:  OriginalEntry    (uint64 LE)
// [13-14]: RepairOpCount    (uint16 LE) = N
// [15+]:   Repair ops (each 24 bytes, in forward order op[0]..op[N-1]):
//          [+0..+7]:   dst_vaddr  = seg.p_vaddr + insertion_rel_offset_after_prior_insertions
//          [+8..+15]:  src_vaddr  = dst_vaddr + insert_size
//          [+16..+23]: move_count = seg.p_filesz - rel_orig_offset
//
// Stub 从 op[N-1] 往前执行到 op[0]，从后往前依次 memmove 删除插入数据。
// repair op 计算（以原始坐标为基准）：
//   rel_orig[j]   = insert_offsets[j] - seg.p_offset   （插入前相对于段首的偏移）
//   cumulative[j] = sum(insert_sizes[k] for k < j in same segment)
//   actual_rel[j] = rel_orig[j] + cumulative[j]        （在已扩展内存中的实际偏移）
//   dst_vaddr     = seg.p_vaddr + actual_rel[j]
//   src_vaddr     = dst_vaddr + insert_sizes[j]
//   move_count    = seg.p_filesz - rel_orig[j]
std::vector<uint8_t> StubGenerator::generate_metadata(
    const std::vector<uint64_t>& insert_offsets,
    const std::vector<uint64_t>& insert_sizes,
    uint64_t original_entry,
    const std::vector<SegmentInfo>& segments) {
    
    std::vector<uint8_t> metadata;

    // 预先收集所有 repair ops（按照 insert_offsets 原始顺序，分段计算累积偏移）
    struct RepairOp {
        uint64_t dst_vaddr;
        uint64_t src_vaddr;
        uint64_t move_count;
    };
    std::vector<RepairOp> ops;

    for (const auto& seg : segments) {
        if (!seg.is_load() || seg.p_filesz == 0) {
            continue;
        }

        // 收集属于该段的插入（按文件偏移排序）
        struct SegInsertion {
            size_t global_idx;   // 在 insert_offsets 中的下标
            uint64_t file_off;   // 文件偏移（原始坐标）
        };
        std::vector<SegInsertion> seg_ins;
        for (size_t i = 0; i < insert_offsets.size(); ++i) {
            const uint64_t off = insert_offsets[i];
            if (off >= seg.p_offset && off < seg.p_offset + seg.p_filesz) {
                seg_ins.push_back({i, off});
            }
        }
        if (seg_ins.empty()) {
            continue;
        }
        std::sort(seg_ins.begin(), seg_ins.end(),
                  [](const SegInsertion& a, const SegInsertion& b) {
                      return a.file_off < b.file_off;
                  });

        uint64_t cumulative_shift = 0;
        for (const auto& si : seg_ins) {
            const uint64_t rel_orig  = si.file_off - seg.p_offset;
            const uint64_t actual_rel = rel_orig + cumulative_shift;
            const uint64_t ins_size   = insert_sizes[si.global_idx];

            RepairOp op;
            op.dst_vaddr  = seg.p_vaddr + actual_rel;
            op.src_vaddr  = op.dst_vaddr + ins_size;
            op.move_count = seg.p_filesz - rel_orig;
            ops.push_back(op);

            cumulative_shift += ins_size;
        }
    }

    // 若没有 segments 信息（或没有匹配的段），回退到旧的简单格式
    // 在这种情况下，stub 也没有足够信息恢复，仅写入 count=0
    const uint16_t op_count = static_cast<uint16_t>(
        ops.size() <= 0xFFFF ? ops.size() : 0xFFFF);

    // Magic 'E','L','F','S'
    metadata.push_back(0x45); // 'E'
    metadata.push_back(0x4C); // 'L'
    metadata.push_back(0x46); // 'F'
    metadata.push_back(0x53); // 'S'
    // Version
    metadata.push_back(0x01);
    // OriginalEntry (uint64 LE)
    uint8_t entry_buf[8];
    write_u64(entry_buf, original_entry, true);
    metadata.insert(metadata.end(), entry_buf, entry_buf + 8);
    // RepairOpCount (uint16 LE)
    uint8_t count_buf[2];
    write_u16(count_buf, op_count, true);
    metadata.insert(metadata.end(), count_buf, count_buf + 2);
    // Repair ops
    for (uint16_t i = 0; i < op_count; ++i) {
        uint8_t buf[8];
        write_u64(buf, ops[i].dst_vaddr, true);
        metadata.insert(metadata.end(), buf, buf + 8);
        write_u64(buf, ops[i].src_vaddr, true);
        metadata.insert(metadata.end(), buf, buf + 8);
        write_u64(buf, ops[i].move_count, true);
        metadata.insert(metadata.end(), buf, buf + 8);
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

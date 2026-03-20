#ifndef __STUB_GENERATOR_H__
#define __STUB_GENERATOR_H__

#include "common.h"
#include <vector>
#include <cstdint>
#include <string>

// Stub 类型
enum class StubType {
    RESTORE_DATA,      // 恢复插入的数据
    DECRYPT,           // 解密代码
    DECOMPRESS,        // 解压代码（类似 UPX）
    CUSTOM,            // 自定义 stub
};

// Stub 信息
struct StubInfo {
    StubType type;
    std::string name;
    std::vector<uint8_t> code;      // Stub 机器码
    uint64_t size;                   // Stub 大小
    uint64_t entry_offset;          // Stub 入口偏移
    uint64_t original_entry;        // 原始入口点
    std::vector<uint8_t> metadata;  // Stub 元数据（恢复信息）
};

// Stub 生成器
class StubGenerator {
public:
    StubGenerator(Architecture arch);
    ~StubGenerator();
    
    // 生成恢复 stub（旧接口：简单入口跳板，元数据不用于执行）
    StubInfo generate_restore_stub(
        const std::vector<uint64_t>& insert_offsets,  // 插入位置（文件偏移）
        const std::vector<uint64_t>& insert_sizes,    // 每次插入大小
        uint64_t original_entry
    );

    // 生成恢复 stub（新接口：在目标进程内存中移除插入数据并跳回原入口）
    StubInfo generate_restore_stub(
        const std::vector<uint64_t>& insert_vaddrs,  // 插入虚拟地址（含累计偏移）
        const std::vector<uint64_t>& insert_sizes,
        const std::vector<uint64_t>& move_lens,      // 每个插入块之后需移动的字节数
        uint64_t stub_vaddr,                          // stub 自身的虚拟地址
        uint64_t original_entry                       // 原始入口虚拟地址
    );
    
    // 生成解密 stub
    StubInfo generate_decrypt_stub(
        uint64_t code_start,
        uint64_t code_size,
        const uint8_t* key,
        size_t key_size,
        uint64_t original_entry
    );
    
    // 生成自定义 stub
    StubInfo generate_custom_stub(
        const std::vector<uint8_t>& stub_code,
        uint64_t original_entry
    );
    
    // 获取架构对应的 NOP 指令
    const ArchNOPInfo* get_nop_info() const { return nop_info; }
    
private:
    Architecture arch;
    const ArchNOPInfo* nop_info;
    
    // 内部生成函数
    std::vector<uint8_t> generate_x86_64_restore_stub(
        const std::vector<uint64_t>& insert_offsets,
        const std::vector<uint64_t>& insert_sizes
    );

    // 生成完整 x86_64 恢复 stub（含 mprotect+memmove 循环）
    std::vector<uint8_t> generate_x86_64_restore_stub_v2();

    std::vector<uint8_t> generate_arm64_restore_stub(
        const std::vector<uint64_t>& insert_offsets,
        const std::vector<uint64_t>& insert_sizes
    );
    
    std::vector<uint8_t> generate_arm_restore_stub(
        const std::vector<uint64_t>& insert_offsets,
        const std::vector<uint64_t>& insert_sizes
    );
    
    // 生成元数据（旧格式）
    std::vector<uint8_t> generate_metadata(
        const std::vector<uint64_t>& insert_offsets,
        const std::vector<uint64_t>& insert_sizes
    );

    // 生成元数据（新格式：entry_delta, count, per-insertion records）
    std::vector<uint8_t> generate_metadata_v2(
        const std::vector<uint64_t>& insert_vaddrs,
        const std::vector<uint64_t>& insert_sizes,
        const std::vector<uint64_t>& move_lens,
        uint64_t stub_vaddr,
        uint64_t original_entry
    );
};

#endif // __STUB_GENERATOR_H__
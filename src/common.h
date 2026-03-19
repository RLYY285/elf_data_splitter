#ifndef __COMMON_H__
#define __COMMON_H__

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <elf.h>

// 架构定义
enum class Architecture {
    UNKNOWN = 0,
    X86,
    X86_64,
    ARM,
    ARM64,
    MIPS,
    MIPS64,
};

// NOP 指令定义（每个架构）
struct ArchNOPInfo {
    Architecture arch;
    uint8_t nop_bytes[8];
    size_t nop_size;
};

// 全局 NOP 指令表
extern const ArchNOPInfo NOP_INSTRUCTIONS[];
extern const size_t NOP_INSTRUCTIONS_COUNT;

// 根据 ELF 机器类型获取架构
Architecture get_architecture(uint16_t e_machine);

// 根据架构获取 NOP 指令
const ArchNOPInfo* get_nop_info(Architecture arch);

// 日志级别
enum class LogLevel {
    ERROR,
    WARN,
    INFO,
    DEBUG,
    TRACE
};

// 全局日志级别
extern LogLevel g_log_level;

// 日志函数
void log_message(LogLevel level, const std::string& message);
void log_error(const std::string& msg);
void log_warn(const std::string& msg);
void log_info(const std::string& msg);
void log_debug(const std::string& msg);
void log_trace(const std::string& msg);

// 字节序转换辅助函数
uint16_t read_u16(const uint8_t* data, bool is_little_endian);
uint32_t read_u32(const uint8_t* data, bool is_little_endian);
uint64_t read_u64(const uint8_t* data, bool is_little_endian);

void write_u16(uint8_t* data, uint16_t value, bool is_little_endian);
void write_u32(uint8_t* data, uint32_t value, bool is_little_endian);
void write_u64(uint8_t* data, uint64_t value, bool is_little_endian);

// 字节序检查
bool is_little_endian_native();

// 错误码定义
enum class ErrorCode {
    SUCCESS = 0,
    FILE_NOT_FOUND = 1,
    INVALID_ELF_FORMAT = 2,
    UNSUPPORTED_ARCH = 3,
    INVALID_ARGUMENTS = 4,
    FILE_WRITE_ERROR = 5,
    MEMORY_ERROR = 6,
    PROCESSING_ERROR = 7,
};

#endif // __COMMON_H__
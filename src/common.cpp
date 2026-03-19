#include "common.h"
#include <iostream>
#include <cstring>

// 日志级别
LogLevel g_log_level = LogLevel::INFO;

// NOP 指令表
const ArchNOPInfo NOP_INSTRUCTIONS[] = {
    {Architecture::X86, {0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 1},
    {Architecture::X86_64, {0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 1},
    {Architecture::ARM, {0x00, 0x00, 0xa0, 0xe1, 0x00, 0x00, 0x00, 0x00}, 4},
    {Architecture::ARM64, {0x1f, 0x20, 0x03, 0xd5, 0x00, 0x00, 0x00, 0x00}, 4},
    {Architecture::MIPS, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 4},
    {Architecture::MIPS64, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 4},
};

const size_t NOP_INSTRUCTIONS_COUNT = sizeof(NOP_INSTRUCTIONS) / sizeof(NOP_INSTRUCTIONS[0]);

Architecture get_architecture(uint16_t e_machine) {
    switch (e_machine) {
        case EM_386:
            return Architecture::X86;
        case EM_X86_64:
            return Architecture::X86_64;
        case EM_ARM:
            return Architecture::ARM;
        case EM_AARCH64:
            return Architecture::ARM64;
        case EM_MIPS:
            return Architecture::MIPS;
        case EM_MIPS_RS3_LE:
            return Architecture::MIPS;
        default:
            return Architecture::UNKNOWN;
    }
}

const ArchNOPInfo* get_nop_info(Architecture arch) {
    for (size_t i = 0; i < NOP_INSTRUCTIONS_COUNT; ++i) {
        if (NOP_INSTRUCTIONS[i].arch == arch) {
            return &NOP_INSTRUCTIONS[i];
        }
    }
    return nullptr;
}

void log_message(LogLevel level, const std::string& message) {
    if (level > g_log_level) {
        return;
    }

    const char* level_str = "";
    switch (level) {
        case LogLevel::ERROR:
            level_str = "[ERROR]";
            break;
        case LogLevel::WARN:
            level_str = "[WARN]";
            break;
        case LogLevel::INFO:
            level_str = "[INFO]";
            break;
        case LogLevel::DEBUG:
            level_str = "[DEBUG]";
            break;
        case LogLevel::TRACE:
            level_str = "[TRACE]";
            break;
    }

    std::cerr << level_str << " " << message << std::endl;
}

void log_error(const std::string& msg) { log_message(LogLevel::ERROR, msg); }
void log_warn(const std::string& msg) { log_message(LogLevel::WARN, msg); }
void log_info(const std::string& msg) { log_message(LogLevel::INFO, msg); }
void log_debug(const std::string& msg) { log_message(LogLevel::DEBUG, msg); }
void log_trace(const std::string& msg) { log_message(LogLevel::TRACE, msg); }

uint16_t read_u16(const uint8_t* data, bool is_little_endian) {
    if (is_little_endian) {
        return data[0] | (data[1] << 8);
    }
    return (data[0] << 8) | data[1];
}

uint32_t read_u32(const uint8_t* data, bool is_little_endian) {
    if (is_little_endian) {
        return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
    }
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

uint64_t read_u64(const uint8_t* data, bool is_little_endian) {
    if (is_little_endian) {
        return (uint64_t)data[0] | ((uint64_t)data[1] << 8) | ((uint64_t)data[2] << 16) |
               ((uint64_t)data[3] << 24) | ((uint64_t)data[4] << 32) | ((uint64_t)data[5] << 40) |
               ((uint64_t)data[6] << 48) | ((uint64_t)data[7] << 56);
    }
    return ((uint64_t)data[0] << 56) | ((uint64_t)data[1] << 48) | ((uint64_t)data[2] << 40) |
           ((uint64_t)data[3] << 32) | ((uint64_t)data[4] << 24) | ((uint64_t)data[5] << 16) |
           ((uint64_t)data[6] << 8) | (uint64_t)data[7];
}

void write_u16(uint8_t* data, uint16_t value, bool is_little_endian) {
    if (is_little_endian) {
        data[0] = value & 0xFF;
        data[1] = (value >> 8) & 0xFF;
    } else {
        data[0] = (value >> 8) & 0xFF;
        data[1] = value & 0xFF;
    }
}

void write_u32(uint8_t* data, uint32_t value, bool is_little_endian) {
    if (is_little_endian) {
        data[0] = value & 0xFF;
        data[1] = (value >> 8) & 0xFF;
        data[2] = (value >> 16) & 0xFF;
        data[3] = (value >> 24) & 0xFF;
    } else {
        data[0] = (value >> 24) & 0xFF;
        data[1] = (value >> 16) & 0xFF;
        data[2] = (value >> 8) & 0xFF;
        data[3] = value & 0xFF;
    }
}

void write_u64(uint8_t* data, uint64_t value, bool is_little_endian) {
    if (is_little_endian) {
        data[0] = value & 0xFF;
        data[1] = (value >> 8) & 0xFF;
        data[2] = (value >> 16) & 0xFF;
        data[3] = (value >> 24) & 0xFF;
        data[4] = (value >> 32) & 0xFF;
        data[5] = (value >> 40) & 0xFF;
        data[6] = (value >> 48) & 0xFF;
        data[7] = (value >> 56) & 0xFF;
    } else {
        data[0] = (value >> 56) & 0xFF;
        data[1] = (value >> 48) & 0xFF;
        data[2] = (value >> 40) & 0xFF;
        data[3] = (value >> 32) & 0xFF;
        data[4] = (value >> 24) & 0xFF;
        data[5] = (value >> 16) & 0xFF;
        data[6] = (value >> 8) & 0xFF;
        data[7] = value & 0xFF;
    }
}

bool is_little_endian_native() {
    uint32_t x = 1;
    return *(uint8_t*)&x == 1;
}
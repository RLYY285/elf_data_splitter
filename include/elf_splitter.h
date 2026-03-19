#ifndef __ELF_SPLITTER_PUBLIC_H__
#define __ELF_SPLITTER_PUBLIC_H__

#include <cstdint>
#include <string>
#include <vector>

// 公共 API 头文件 - 供外部使用

namespace elf_splitter {

// 架构枚举
enum class Architecture {
    UNKNOWN = 0,
    X86,
    X86_64,
    ARM,
    ARM64,
    MIPS,
    MIPS64,
};

// 处理选项结构
struct SplitterOptions {
    size_t interval;           // 切割间隔（字节数）
    size_t insert_size;        // 每次插入的字节数
    bool use_nop;              // 是否使用 NOP 填充
    Architecture arch;         // 目标架构
    bool verbose;              // 是否输出详细日志
};

// 处理结果统计
struct ProcessStatistics {
    size_t total_segments_processed;  // 处理的段数
    size_t total_bytes_inserted;      // 插入的总字节数
    size_t segments_skipped;          // 跳过的段数
    std::string summary;              // 处理摘要
};

// 错误码
enum class ErrorCode {
    SUCCESS = 0,
    FILE_NOT_FOUND = 1,
    INVALID_ELF_FORMAT = 2,
    UNSUPPORTED_ARCH = 3,
    INVALID_ARGUMENTS = 4,
    FILE_WRITE_ERROR = 5,
    MEMORY_ERROR = 6,
    PROCESSING_ERROR = 7,
    BOUNDARY_VIOLATION = 8,
};

// 主处理类 - C++ 库接口
class Splitter {
public:
    Splitter();
    ~Splitter();
    
    // 设置处理选项
    void set_options(const SplitterOptions& options);
    
    // 加载 ELF 文件
    ErrorCode load_elf(const std::string& input_file);
    
    // 处理 ELF 文件
    ErrorCode process();
    
    // 保存处理后的 ELF 文件
    ErrorCode save_elf(const std::string& output_file);
    
    // 一步到位：加载、处理、保存
    ErrorCode process_file(
        const std::string& input_file,
        const std::string& output_file,
        const SplitterOptions& options
    );
    
    // 仅分析文件（不处理）
    ErrorCode analyze_elf(const std::string& input_file);
    
    // 获取处理统计信息
    const ProcessStatistics& get_statistics() const;
    
    // 获取最后一个错误信息
    const std::string& get_last_error() const;
    
    // 将错误码转换为字符串
    static std::string error_code_to_string(ErrorCode code);

private:
    class Impl;
    Impl* pImpl;
};

// C 风格 API - 用于 C 代码或其他语言绑定
extern "C" {
    // 创建分割器实例
    void* splitter_create();
    
    // 销毁分割器实例
    void splitter_destroy(void* handle);
    
    // 处理文件
    int splitter_process_file(
        void* handle,
        const char* input_file,
        const char* output_file,
        size_t interval,
        size_t insert_size,
        int use_nop
    );
    
    // 获取错误信息
    const char* splitter_get_error(void* handle);
    
    // 设置日志级别
    void splitter_set_verbose(int verbose);
}

} // namespace elf_splitter

#endif // __ELF_SPLITTER_PUBLIC_H__
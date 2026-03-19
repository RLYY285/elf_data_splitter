#include "elf_splitter.h"
#include <iostream>
#include <string>
#include <cstring>

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n"
              << "  -i, --input FILE          Input ELF file\n"
              << "  -o, --output FILE         Output ELF file\n"
              << "  -a, --interval N          Insertion interval (bytes)\n"
              << "  -b, --insert-size N       Bytes to insert per interval\n"
              << "  --fill METHOD             Fill method: zero or nop (default: zero)\n"
              << "  --analyze                 Analyze only, do not generate output\n"
              << "  -v, --verbose             Verbose output\n"
              << "  -h, --help                Show this help\n";
}

int main(int argc, char* argv[]) {
    std::string input_file;
    std::string output_file;
    size_t interval = 256;
    size_t insert_size = 16;
    bool use_nop = false;
    bool analyze_only = false;
    bool verbose = false;
    
    // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-i" || arg == "--input") {
            if (i + 1 < argc) {
                input_file = argv[++i];
            }
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                output_file = argv[++i];
            }
        } else if (arg == "-a" || arg == "--interval") {
            if (i + 1 < argc) {
                interval = std::stoull(argv[++i]);
            }
        } else if (arg == "-b" || arg == "--insert-size") {
            if (i + 1 < argc) {
                insert_size = std::stoull(argv[++i]);
            }
        } else if (arg == "--fill") {
            if (i + 1 < argc) {
                std::string fill_method = argv[++i];
                use_nop = (fill_method == "nop");
            }
        } else if (arg == "--analyze") {
            analyze_only = true;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        }
    }
    
    if (verbose) {
        g_log_level = LogLevel::DEBUG;
    }
    
    if (input_file.empty()) {
        std::cerr << "Error: Input file required\n";
        print_usage(argv[0]);
        return 1;
    }
    
    // 创建分割器
    ElfSplitter splitter;
    
    // 自动检测架构
    Architecture arch = Architecture::UNKNOWN;
    
    // 加载 ELF 文件
    if (!splitter.load_elf(input_file)) {
        std::cerr << "Error: " << splitter.get_last_error() << "\n";
        return 1;
    }
    
    // 分析
    if (!splitter.analyze()) {
        std::cerr << "Error: Analysis failed\n";
        return 1;
    }
    
    if (analyze_only) {
        return 0;
    }
    
    if (output_file.empty()) {
        std::cerr << "Error: Output file required for processing\n";
        print_usage(argv[0]);
        return 1;
    }
    
    // 设置选项并处理
    splitter.set_options(interval, insert_size, use_nop, arch);
    
    if (!splitter.process()) {
        std::cerr << "Error: " << splitter.get_last_error() << "\n";
        return 1;
    }
    
    // 保存
    if (!splitter.save_elf(output_file)) {
        std::cerr << "Error: Failed to save output file\n";
        return 1;
    }
    
    const auto& stats = splitter.get_statistics();
    std::cout << stats.summary << "\n";
    
    return 0;
}
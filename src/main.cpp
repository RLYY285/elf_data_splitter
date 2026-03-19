#include "elf_splitter.h"
#include <iostream>
#include <string>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <system_error>
#include <utility>

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n"
              << "  -i, --input FILE          Input ELF file\n"
              << "  -o, --output FILE         Output ELF file\n"
              << "  --manual                  Use manual -a/-b parameters (default: auto optimize)\n"
              << "  -a, --interval N          Insertion interval (bytes, manual mode only)\n"
              << "  -b, --insert-size N       Bytes per insertion (manual mode only)\n"
              << "  --fill METHOD             Fill method: zero or nop (default: zero)\n"
              << "  --allow-exec-segment-edit Allow editing executable PT_LOAD segments (default)\n"
              << "  --skip-exec-segment-edit  Skip executable PT_LOAD segments (safer)\n"
              << "  --inject-stub             Inject stub after data insertion\n"
              << "  --stub-type TYPE          Stub type: restore or custom (default: restore)\n"
              << "  --custom-stub FILE        Custom stub raw bytes file (for --stub-type custom)\n"
              << "  --stub-update-entry       Update ELF entry point to injected stub\n"
              << "  --analyze                 Analyze only, do not generate output\n"
              << "  -v, --verbose             Verbose output\n"
              << "  -h, --help                Show this help\n";
}

bool load_binary_file(
    const std::string& path,
    std::vector<uint8_t>& output,
    std::string& error_msg) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        error_msg = "Failed to open custom stub file: " + path;
        return false;
    }

    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    if (size < 0) {
        error_msg = "Failed to determine custom stub file size: " + path;
        return false;
    }
    in.seekg(0, std::ios::beg);

    output.resize(static_cast<size_t>(size));
    if (!output.empty()) {
        in.read(reinterpret_cast<char*>(output.data()), size);
        if (!in) {
            error_msg = "Failed to read custom stub file: " + path;
            return false;
        }
    }

    return true;
}

int main(int argc, char* argv[]) {
    std::string input_file;
    std::string output_file;
    size_t interval = 0;
    size_t insert_size = 0;
    bool use_nop = false;
    bool analyze_only = false;
    bool verbose = false;
    bool auto_optimize = true;
    bool allow_exec_segment_edit = true;
    bool inject_stub = false;
    bool stub_update_entry = false;
    StubType stub_type = StubType::RESTORE_DATA;
    std::string custom_stub_file;
    
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
        } else if (arg == "--manual") {
            auto_optimize = false;
        } else if (arg == "--fill") {
            if (i + 1 < argc) {
                std::string fill_method = argv[++i];
                use_nop = (fill_method == "nop");
            }
        } else if (arg == "--allow-exec-segment-edit") {
            allow_exec_segment_edit = true;
        } else if (arg == "--skip-exec-segment-edit") {
            allow_exec_segment_edit = false;
        } else if (arg == "--inject-stub") {
            inject_stub = true;
        } else if (arg == "--stub-update-entry") {
            stub_update_entry = true;
        } else if (arg == "--stub-type") {
            if (i + 1 < argc) {
                const std::string stub_type_value = argv[++i];
                if (stub_type_value == "restore") {
                    stub_type = StubType::RESTORE_DATA;
                } else if (stub_type_value == "custom") {
                    stub_type = StubType::CUSTOM;
                } else {
                    std::cerr << "Error: Unsupported stub type: " << stub_type_value << "\n";
                    return 1;
                }
            }
        } else if (arg == "--custom-stub") {
            if (i + 1 < argc) {
                custom_stub_file = argv[++i];
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

    if (!auto_optimize && (interval == 0 || insert_size == 0)) {
        std::cerr << "Error: Manual mode requires both -a and -b, and both must be > 0\n";
        return 1;
    }

    std::vector<uint8_t> custom_stub_code;
    if (inject_stub && stub_type == StubType::CUSTOM) {
        if (custom_stub_file.empty()) {
            std::cerr << "Error: --stub-type custom requires --custom-stub FILE\n";
            return 1;
        }
        std::string load_error;
        if (!load_binary_file(custom_stub_file, custom_stub_code, load_error)) {
            std::cerr << "Error: " << load_error << "\n";
            return 1;
        }
        if (custom_stub_code.empty()) {
            std::cerr << "Error: Custom stub file is empty\n";
            return 1;
        }
    }
    
    // 设置选项并处理
    ElfSplitterOptions options{};
    options.interval = interval;
    options.insert_size = insert_size;
    options.use_nop = use_nop;
    options.arch = arch;
    options.auto_optimize = auto_optimize;
    options.allow_exec_segment_edit = allow_exec_segment_edit;
    options.inject_stub = inject_stub;
    options.stub_type = stub_type;
    options.update_entry_point = stub_update_entry;
    options.custom_stub_code = std::move(custom_stub_code);
    splitter.set_options(options);
    
    if (!splitter.process()) {
        std::cerr << "Error: " << splitter.get_last_error() << "\n";
        return 1;
    }

    // 自动创建输出目录（如果指定了父目录）
    const std::filesystem::path output_path(output_file);
    const std::filesystem::path output_parent = output_path.parent_path();
    if (!output_parent.empty()) {
        std::error_code mkdir_ec;
        std::filesystem::create_directories(output_parent, mkdir_ec);
        if (mkdir_ec) {
            std::cerr << "Error: Failed to create output directory: "
                      << output_parent.string() << " (" << mkdir_ec.message() << ")\n";
            return 1;
        }
    }
    
    // 保存
    if (!splitter.save_elf(output_file)) {
        std::cerr << "Error: Failed to save output file\n";
        return 1;
    }

    std::error_code perm_ec;
    const auto input_status = std::filesystem::status(input_file, perm_ec);
    if (!perm_ec) {
        std::filesystem::permissions(
            output_file,
            input_status.permissions(),
            std::filesystem::perm_options::replace,
            perm_ec);
    }
    
    const auto& stats = splitter.get_statistics();
    std::cout << stats.summary << "\n";
    
    return 0;
}

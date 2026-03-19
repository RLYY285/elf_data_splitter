#include "common.h"
#include "elf_parser.h"
#include "elf_splitter.h"
#include "segment_handler.h"
#include "offset_calculator.h"
#include <iostream>
#include <cassert>
#include <vector>

void test_architecture_detection() {
    std::cout << "Testing architecture detection...\n";
    
    Architecture arch_x86 = get_architecture(EM_386);
    assert(arch_x86 == Architecture::X86);
    
    Architecture arch_x64 = get_architecture(EM_X86_64);
    assert(arch_x64 == Architecture::X86_64);
    
    Architecture arch_arm = get_architecture(EM_ARM);
    assert(arch_arm == Architecture::ARM);
    
    std::cout << "  PASSED\n";
}

void test_nop_info() {
    std::cout << "Testing NOP instruction info...\n";
    
    const ArchNOPInfo* nop_x86 = get_nop_info(Architecture::X86);
    assert(nop_x86 != nullptr);
    assert(nop_x86->nop_size == 1);
    assert(nop_x86->nop_bytes[0] == 0x90);
    
    const ArchNOPInfo* nop_arm = get_nop_info(Architecture::ARM);
    assert(nop_arm != nullptr);
    assert(nop_arm->nop_size == 4);
    
    std::cout << "  PASSED\n";
}

void test_offset_calculator() {
    std::cout << "Testing offset calculator...\n";
    
    OffsetCalculator calc;
    calc.add_insertion(256, 16);
    calc.add_insertion(512, 16);
    
    uint64_t new_offset = calc.calculate_new_offset(768);
    assert(new_offset == 768 + 32);  // 两次插入，共 32 字节
    
    assert(calc.get_total_insertion_size() == 32);
    
    std::cout << "  PASSED\n";
}

void test_segment_auto_optimization() {
    std::cout << "Testing segment auto optimization...\n";

    std::vector<uint8_t> data(130);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(i & 0xFF);
    }

    ProcessOptions options{};
    options.interval = 0;
    options.insert_size = 0;
    options.use_nop = false;
    options.arch = Architecture::X86;
    options.auto_optimize = true;

    SegmentHandler handler(options);
    SegmentProcessResult result = handler.process_segment(data, 0, 100, 130, 0);

    assert(result.success);
    assert(result.original_size == 100);
    assert(result.inserted_bytes == 30);
    assert(result.new_size == 130);
    assert(result.processed_data.size() == 130);
    assert(result.interval_used > 0);
    assert(result.insertion_events > 0);

    std::cout << "  PASSED\n";
}

void test_protected_prefix() {
    std::cout << "Testing protected prefix...\n";

    std::vector<uint8_t> data(80);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>((i * 3) & 0xFF);
    }

    ProcessOptions options{};
    options.interval = 0;
    options.insert_size = 0;
    options.use_nop = false;
    options.arch = Architecture::X86;
    options.auto_optimize = true;

    SegmentHandler handler(options);
    SegmentProcessResult result = handler.process_segment(data, 0, 64, 80, 16);

    assert(result.success);
    assert(result.new_size == 80);
    assert(result.inserted_bytes == 16);

    for (size_t i = 0; i < 16; ++i) {
        assert(result.processed_data[i] == data[i]);
    }
    for (uint64_t point : result.insertion_points) {
        assert(point >= 17);
    }

    std::cout << "  PASSED\n";
}

int main() {
    std::cout << "Running ELF Splitter tests...\n\n";
    
    try {
        test_architecture_detection();
        test_nop_info();
        test_offset_calculator();
        test_segment_auto_optimization();
        test_protected_prefix();
        
        std::cout << "\nAll tests passed!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << "\n";
        return 1;
    }
}

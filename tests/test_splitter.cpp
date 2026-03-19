#include "elf_splitter.h"
#include <iostream>
#include <cassert>

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

int main() {
    std::cout << "Running ELF Splitter tests...\n\n";
    
    try {
        test_architecture_detection();
        test_nop_info();
        test_offset_calculator();
        
        std::cout << "\nAll tests passed!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << "\n";
        return 1;
    }
}
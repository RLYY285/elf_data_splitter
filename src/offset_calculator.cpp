#include "offset_calculator.h"
#include <algorithm>

OffsetCalculator::OffsetCalculator() : total_insertion_size(0) {}

OffsetCalculator::~OffsetCalculator() {}

void OffsetCalculator::add_insertion(uint64_t offset, uint64_t insert_size) {
    OffsetMapping mapping;
    mapping.original_offset = offset;
    mapping.size = insert_size;
    mapping.new_offset = offset + total_insertion_size;
    
    mappings.push_back(mapping);
    total_insertion_size += insert_size;
}

uint64_t OffsetCalculator::calculate_new_offset(uint64_t original_offset) const {
    uint64_t additional_offset = 0;
    
    for (const auto& mapping : mappings) {
        if (mapping.original_offset <= original_offset) {
            additional_offset += mapping.size;
        } else {
            break;
        }
    }
    
    return original_offset + additional_offset;
}

void OffsetCalculator::reset() {
    mappings.clear();
    total_insertion_size = 0;
}
#ifndef __OFFSET_CALCULATOR_H__
#define __OFFSET_CALCULATOR_H__

#include <cstdint>
#include <vector>
#include <map>

// 偏移量映射信息
struct OffsetMapping {
    uint64_t original_offset;  // 原始偏移
    uint64_t new_offset;       // 新偏移
    uint64_t size;             // 数据大小
};

// 偏移量计算器 - 处理因插入数据导致的偏移变化
class OffsetCalculator {
public:
    OffsetCalculator();
    ~OffsetCalculator();
    
    // 添加一个插入操作记录
    void add_insertion(uint64_t offset, uint64_t insert_size);
    
    // 计算新的偏移量
    uint64_t calculate_new_offset(uint64_t original_offset) const;
    
    // 计算总共插入的数据量
    uint64_t get_total_insertion_size() const { return total_insertion_size; }
    
    // 重置计算器
    void reset();
    
    // 获取所有映射
    const std::vector<OffsetMapping>& get_mappings() const { return mappings; }
    
private:
    std::vector<OffsetMapping> mappings;  // 排序的映射列表
    uint64_t total_insertion_size;        // 总插入大小
};

#endif // __OFFSET_CALCULATOR_H__
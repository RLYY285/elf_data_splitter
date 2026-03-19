# ELF Data Splitter

基于 UPX 处理思路的 ELF 文件数据切割和插入工具。

## 功能特性

- ✅ 按 "每隔 a 字节插入 b 字节" 的模式处理 ELF 数据
- ✅ 支持填充 0 或 NOP 指令
- ✅ 区分 ET_EXEC 和 ET_DYN 文件类型
- ✅ 自动约束处理防止段越界
- ✅ 支持 32-bit 和 64-bit ELF
- ✅ 支持多种架构（x86, x86_64, ARM, ARM64, MIPS）
- ✅ 详细的日志和分析输出

## 编译

### 使用 CMake

```bash
mkdir build
cd build
cmake ..
make
```

### 编译并运行测试

```bash
make test
```

## 使用

### 基础用法

```bash
./elf-splitter -i input.elf -o output.elf -a 256 -b 16 --fill zero
```

### 参数说明

| 参数 | 说明 | 示例 |
|------|------|------|
| `-i, --input FILE` | 输入 ELF 文件 | `-i app` |
| `-o, --output FILE` | 输出 ELF 文件 | `-o app.split` |
| `-a, --interval N` | 切割间隔（字节） | `-a 512` |
| `-b, --insert-size N` | 每次插入的字节数 | `-b 32` |
| `--fill METHOD` | 填充方式（zero/nop） | `--fill nop` |
| `--analyze` | 仅分析，不输出文件 | `--analyze` |
| `-v, --
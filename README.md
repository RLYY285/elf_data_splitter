# ELF Data Splitter

ELF 文件段内数据插入工具。当前版本支持真实改写并输出新 ELF，默认使用自动优化模式为每个 `PT_LOAD` 段选择插入参数。

## 功能概览

- 默认自动优化：自动计算每段的 `interval/insert_size`，在可用空间内尽可能多插入
- 手动模式：使用 `--manual -a <interval> -b <insert_size>`
- 填充模式：`zero` 或 `nop`（`--fill`）
- 默认允许处理可执行 `PT_LOAD` 段（保持原始切割/插入逻辑）
- 可选跳过可执行段：`--skip-exec-segment-edit`
- 支持可选 stub 注入（`--inject-stub`）
- 未开启 `--stub-update-entry` 时，stub 默认追加到文件末尾
- `restore` 模式会先按记录修复可执行段（静态修复），再追加 stub+metadata
- 支持 `ET_EXEC` / `ET_DYN`，支持 32-bit / 64-bit ELF
- 处理后会更新关键头字段（如 `p_filesz`、`p_memsz`、`e_entry`、section 偏移/地址）
- 输出路径的父目录不存在时会自动创建
- 支持仅分析模式（`--analyze`）

## 编译

```bash
mkdir -p build
cd build
cmake ..
make -j
```

构建成功后可执行文件为 `build/elf-splitter`。

## 测试

```bash
cd build
ctest --output-on-failure
```

## 命令行用法

```text
Usage: ./elf-splitter [OPTIONS]
  -i, --input FILE          Input ELF file
  -o, --output FILE         Output ELF file
  --manual                  Use manual -a/-b parameters (default: auto optimize)
  -a, --interval N          Insertion interval (bytes, manual mode only)
  -b, --insert-size N       Bytes per insertion (manual mode only)
  --fill METHOD             Fill method: zero or nop (default: zero)
  --allow-exec-segment-edit Allow editing executable PT_LOAD segments (default)
  --skip-exec-segment-edit  Skip executable PT_LOAD segments (safer)
  --inject-stub             Inject stub after data insertion
  --stub-type TYPE          Stub type: restore or custom (default: restore)
  --custom-stub FILE        Custom stub raw bytes file (for --stub-type custom)
  --stub-update-entry       Update ELF entry point to injected stub
  --analyze                 Analyze only, do not generate output
  -v, --verbose             Verbose output
  -h, --help                Show this help
```

## 常见示例

1. 默认自动优化（推荐）

```bash
cd build
./elf-splitter -i /path/to/input.elf -o /path/to/output.elf --fill zero
```

2. 手动模式（必须同时提供 `-a` 和 `-b`，且都大于 0）

```bash
cd build
./elf-splitter --manual -i /path/to/input.elf -o /path/to/output.elf -a 256 -b 16 --fill nop
```

3. 仅分析（不写输出文件）

```bash
cd build
./elf-splitter -i /path/to/input.elf --analyze
```

4. 启用恢复 stub 注入（默认不改入口点）

```bash
cd build
./elf-splitter -i /path/to/input.elf -o /path/to/output.elf --inject-stub
```

5. 启用 restore stub 并将入口点切到 stub（x86_64）

```bash
cd build
./elf-splitter -i /path/to/input.elf -o /path/to/output.elf \
  --inject-stub --stub-type restore --stub-update-entry
```

6. 注入自定义 stub（二进制原始字节）

```bash
cd build
./elf-splitter -i /path/to/input.elf -o /path/to/output.elf \
  --inject-stub --stub-type custom --custom-stub /path/to/stub.bin
```

## 参数说明

| 参数 | 说明 |
|---|---|
| `-i, --input FILE` | 输入 ELF 文件（必填） |
| `-o, --output FILE` | 输出 ELF 文件（处理模式必填，`--analyze` 时不需要） |
| `--manual` | 关闭自动优化，启用手动参数 |
| `-a, --interval N` | 插入间隔，仅手动模式有效 |
| `-b, --insert-size N` | 每次插入字节数，仅手动模式有效 |
| `--fill METHOD` | `zero` 或 `nop`，默认 `zero` |
| `--allow-exec-segment-edit` | 允许改写可执行段（默认） |
| `--skip-exec-segment-edit` | 跳过可执行段（更安全） |
| `--inject-stub` | 启用 stub 注入（在切割处理后执行） |
| `--stub-type TYPE` | `restore` 或 `custom`，默认 `restore` |
| `--custom-stub FILE` | 自定义 stub 字节文件（`--stub-type custom` 时必填） |
| `--stub-update-entry` | 将入口点改到注入的 restore stub（当前仅建议 x86_64 + restore） |
| `--analyze` | 仅分析，不生成输出 |
| `-v, --verbose` | 打开调试级别日志 |
| `-h, --help` | 显示帮助 |

## 已知边界

- 当前目标是“结构层面的插入与头部修正”，不是“保持二进制行为等价”
- 对可执行段插入后，输出文件可能无法正常运行；可使用 `--skip-exec-segment-edit`
- `process_segment_gaps()` 目前是预留扩展点
- `restore` 模式目前采用“静态修复 + 入口跳板”方案；更复杂的运行时恢复逻辑仍可继续增强

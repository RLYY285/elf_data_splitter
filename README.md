# ELF Data Splitter

ELF 文件段内数据插入工具。当前版本支持真实改写并输出新 ELF，默认使用自动优化模式为每个 `PT_LOAD` 段选择插入参数，并支持运行时恢复 stub 的生成与注入。

## 功能概览

- 默认自动优化：自动计算每段的 `interval/insert_size`，在可用空间内尽可能多插入
- 手动模式：使用 `--manual -a <interval> -b <insert_size>`
- 填充模式：`zero` 或 `nop`（`--fill`）
- 默认允许处理可执行 `PT_LOAD` 段（保持原始切割/插入逻辑）
- 可选跳过可执行段：`--skip-exec-segment-edit`
- 支持可选 stub 注入（`--inject-stub`）
  - `restore` 模式生成完整 x86_64 运行时恢复 stub（121 字节），紧跟二进制格式元数据
  - 可执行段插入在注入前静态修复；stub 元数据仅记录非可执行段的修复操作
  - `--stub-update-entry` 将 ELF 入口点切换到 stub（仅支持 x86_64 + `restore` 模式）
  - `custom` 模式注入用户自定义原始 stub 字节文件
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

4. 仅切割 + 注入恢复 stub（不改入口点）

```bash
cd build
./elf-splitter -i /path/to/input.elf -o /path/to/output.elf --inject-stub
```

5. 切割 + 恢复 stub + 将入口点切换到 stub（x86_64，ET_EXEC，需要足够 slack 空间）

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
| `--stub-update-entry` | 将入口点改到注入的 restore stub（当前仅支持 x86_64 + restore） |
| `--analyze` | 仅分析，不生成输出 |
| `-v, --verbose` | 打开调试级别日志 |
| `-h, --help` | 显示帮助 |

## Restore Stub 技术细节

### 运行时恢复流程（x86_64，121 字节）

当程序被系统加载后，restore stub 作为新入口点执行：

1. 保存所有通用寄存器及 rflags（`push` 序列）
2. 通过 `lea rbx, [rip+0x5F]` 定位紧跟 stub 代码后的元数据
3. 验证元数据 magic 字段（`"ELFS"`）
4. 从元数据读取原始入口点，写入栈上的 rax 保存槽
5. 从元数据读取 repair op 数量，**从最后一个 op 往前**逐一执行 `rep movsb` 删除插入数据
6. `popfq` + 弹出所有寄存器（rax = 原始入口点）
7. `jmp rax` 跳转到原始入口点

### 元数据二进制格式

紧跟 stub 代码（121 字节）之后：

```
偏移    大小    字段
0       4       Magic: 'E','L','F','S'
4       1       Version: 0x01
5       8       OriginalEntry (uint64, little-endian)
13      2       RepairOpCount N (uint16, little-endian)
15      N×24    Repair ops（见下方）
```

每个 Repair op（24 字节，正向顺序存储，执行时从后往前）：

```
偏移    大小    字段
0       8       dst_vaddr  = seg.p_vaddr + actual_rel_offset
8       8       src_vaddr  = dst_vaddr + insert_size
16      8       move_count = seg.p_filesz - rel_orig_offset
```

### Repair op 计算规则

对于段内第 j 个插入点（以原始文件坐标为基准）：

- `rel_orig[j]   = insert_offsets[j] - seg.p_offset`
- `cumulative[j] = sum(insert_sizes[k] for k < j, in the same segment)`
- `actual_rel[j] = rel_orig[j] + cumulative[j]`
- `dst_vaddr     = seg.p_vaddr + actual_rel[j]`
- `src_vaddr     = dst_vaddr + insert_sizes[j]`
- `move_count    = seg.p_filesz - rel_orig[j]`

Stub 从 `op[N-1]` 往前执行到 `op[0]`，确保后插入的数据先移除，保持偏移正确。

## 已知边界

- 当前目标是"结构层面的插入与头部修正"，不是"保持二进制行为等价"
- 对可执行段插入后，输出文件可能无法正常运行；可使用 `--skip-exec-segment-edit`
- **PIE 二进制（ET_DYN）**：当前实现使用段的绝对虚拟地址（`p_vaddr`），在运行时受 ASLR 基址偏移影响，因此 restore stub 的运行时恢复功能对 ET_DYN 二进制暂不适用；建议对 ET_DYN 不开启 `--stub-update-entry`（可在未来通过 RIP 相对计算 load bias 改进）
- `--stub-update-entry` 要求可执行段有足够的 slack 空间放置 stub；否则返回错误
- `process_segment_gaps()` 目前是预留扩展点

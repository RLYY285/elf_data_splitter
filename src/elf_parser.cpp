#include "elf_parser.h"
#include <fstream>
#include <sstream>
#include <cstring>

ElfParser::ElfParser()
    : ei_class(ELFCLASS32), ei_data(ELFDATA2LSB), e_machine(0), e_type(0),
      e_phnum(0), e_shnum(0), e_entry(0), e_phoff(0), e_shoff(0), e_shstrndx(0) {}

ElfParser::~ElfParser() {}

bool ElfParser::load(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        last_error = "Cannot open file: " + filename;
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    file_data.resize(size);
    if (!file.read(reinterpret_cast<char*>(file_data.data()), size)) {
        last_error = "Failed to read file";
        return false;
    }

    file.close();

    if (file_data.size() < EI_NIDENT) {
        last_error = "File too small to be ELF";
        return false;
    }

    // 检查 ELF 魔数
    if (file_data[EI_MAG0] != ELFMAG0 || file_data[EI_MAG1] != ELFMAG1 ||
        file_data[EI_MAG2] != ELFMAG2 || file_data[EI_MAG3] != ELFMAG3) {
        last_error = "Invalid ELF magic number";
        return false;
    }

    ei_class = file_data[EI_CLASS];
    ei_data = file_data[EI_DATA];

    if (!parse_elf_header()) {
        return false;
    }

    if (!parse_program_headers()) {
        return false;
    }

    if (!parse_section_headers()) {
        return false;
    }

    if (!parse_section_names()) {
        return false;
    }

    return true;
}

bool ElfParser::parse_elf_header() {
    bool is_le = (ei_data == ELFDATA2LSB);

    if (is_64bit()) {
        if (file_data.size() < sizeof(Elf64_Ehdr)) {
            last_error = "File too small for 64-bit ELF header";
            return false;
        }
        Elf64_Ehdr* hdr = (Elf64_Ehdr*)file_data.data();
        e_machine = read_u16((uint8_t*)&hdr->e_machine, is_le);
        e_type = read_u16((uint8_t*)&hdr->e_type, is_le);
        e_phnum = read_u16((uint8_t*)&hdr->e_phnum, is_le);
        e_shnum = read_u16((uint8_t*)&hdr->e_shnum, is_le);
        e_entry = read_u64((uint8_t*)&hdr->e_entry, is_le);
        e_phoff = read_u64((uint8_t*)&hdr->e_phoff, is_le);
        e_shoff = read_u64((uint8_t*)&hdr->e_shoff, is_le);
        e_shstrndx = read_u16((uint8_t*)&hdr->e_shstrndx, is_le);
    } else {
        if (file_data.size() < sizeof(Elf32_Ehdr)) {
            last_error = "File too small for 32-bit ELF header";
            return false;
        }
        Elf32_Ehdr* hdr = (Elf32_Ehdr*)file_data.data();
        e_machine = read_u16((uint8_t*)&hdr->e_machine, is_le);
        e_type = read_u16((uint8_t*)&hdr->e_type, is_le);
        e_phnum = read_u16((uint8_t*)&hdr->e_phnum, is_le);
        e_shnum = read_u16((uint8_t*)&hdr->e_shnum, is_le);
        e_entry = read_u32((uint8_t*)&hdr->e_entry, is_le);
        e_phoff = read_u32((uint8_t*)&hdr->e_phoff, is_le);
        e_shoff = read_u32((uint8_t*)&hdr->e_shoff, is_le);
        e_shstrndx = read_u16((uint8_t*)&hdr->e_shstrndx, is_le);
    }

    log_debug("ELF Header: type=" + std::to_string(e_type) + ", machine=" + std::to_string(e_machine) +
              ", phnum=" + std::to_string(e_phnum) + ", shnum=" + std::to_string(e_shnum));

    return true;
}

bool ElfParser::parse_program_headers() {
    bool is_le = (ei_data == ELFDATA2LSB);

    for (uint32_t i = 0; i < e_phnum; ++i) {
        SegmentInfo seg;
        uint64_t phoff;

        if (is_64bit()) {
            phoff = e_phoff + i * sizeof(Elf64_Phdr);
            if (phoff + sizeof(Elf64_Phdr) > file_data.size()) {
                last_error = "Program header out of bounds";
                return false;
            }
            Elf64_Phdr* phdr = (Elf64_Phdr*)(file_data.data() + phoff);
            seg.p_type = read_u32((uint8_t*)&phdr->p_type, is_le);
            seg.p_flags = read_u32((uint8_t*)&phdr->p_flags, is_le);
            seg.p_offset = read_u64((uint8_t*)&phdr->p_offset, is_le);
            seg.p_vaddr = read_u64((uint8_t*)&phdr->p_vaddr, is_le);
            seg.p_paddr = read_u64((uint8_t*)&phdr->p_paddr, is_le);
            seg.p_filesz = read_u64((uint8_t*)&phdr->p_filesz, is_le);
            seg.p_memsz = read_u64((uint8_t*)&phdr->p_memsz, is_le);
            seg.p_align = read_u64((uint8_t*)&phdr->p_align, is_le);
        } else {
            phoff = e_phoff + i * sizeof(Elf32_Phdr);
            if (phoff + sizeof(Elf32_Phdr) > file_data.size()) {
                last_error = "Program header out of bounds";
                return false;
            }
            Elf32_Phdr* phdr = (Elf32_Phdr*)(file_data.data() + phoff);
            seg.p_type = read_u32((uint8_t*)&phdr->p_type, is_le);
            seg.p_flags = read_u32((uint8_t*)&phdr->p_flags, is_le);
            seg.p_offset = read_u32((uint8_t*)&phdr->p_offset, is_le);
            seg.p_vaddr = read_u32((uint8_t*)&phdr->p_vaddr, is_le);
            seg.p_paddr = read_u32((uint8_t*)&phdr->p_paddr, is_le);
            seg.p_filesz = read_u32((uint8_t*)&phdr->p_filesz, is_le);
            seg.p_memsz = read_u32((uint8_t*)&phdr->p_memsz, is_le);
            seg.p_align = read_u32((uint8_t*)&phdr->p_align, is_le);
        }

        segments.push_back(seg);
    }

    log_debug("Parsed " + std::to_string(segments.size()) + " program headers");
    return true;
}

bool ElfParser::parse_section_headers() {
    bool is_le = (ei_data == ELFDATA2LSB);

    for (uint32_t i = 0; i < e_shnum; ++i) {
        SectionInfo sec;
        uint64_t shoff;

        if (is_64bit()) {
            shoff = e_shoff + i * sizeof(Elf64_Shdr);
            if (shoff + sizeof(Elf64_Shdr) > file_data.size()) {
                last_error = "Section header out of bounds";
                return false;
            }
            Elf64_Shdr* shdr = (Elf64_Shdr*)(file_data.data() + shoff);
            sec.sh_type = read_u32((uint8_t*)&shdr->sh_type, is_le);
            sec.sh_offset = read_u64((uint8_t*)&shdr->sh_offset, is_le);
            sec.sh_size = read_u64((uint8_t*)&shdr->sh_size, is_le);
            sec.sh_addr = read_u64((uint8_t*)&shdr->sh_addr, is_le);
        } else {
            shoff = e_shoff + i * sizeof(Elf32_Shdr);
            if (shoff + sizeof(Elf32_Shdr) > file_data.size()) {
                last_error = "Section header out of bounds";
                return false;
            }
            Elf32_Shdr* shdr = (Elf32_Shdr*)(file_data.data() + shoff);
            sec.sh_type = read_u32((uint8_t*)&shdr->sh_type, is_le);
            sec.sh_offset = read_u32((uint8_t*)&shdr->sh_offset, is_le);
            sec.sh_size = read_u32((uint8_t*)&shdr->sh_size, is_le);
            sec.sh_addr = read_u32((uint8_t*)&shdr->sh_addr, is_le);
        }

        sections.push_back(sec);
    }

    log_debug("Parsed " + std::to_string(sections.size()) + " section headers");
    return true;
}

bool ElfParser::parse_section_names() {
    if (e_shstrndx >= sections.size()) {
        log_warn("Invalid section string table index");
        return true;
    }

    const SectionInfo& strtab = sections[e_shstrndx];
    if (strtab.sh_offset + strtab.sh_size > file_data.size()) {
        log_warn("Section string table out of bounds");
        return true;
    }

    const char* strtab_data = (const char*)(file_data.data() + strtab.sh_offset);

    for (auto& sec : sections) {
        uint32_t name_offset = 0; // 这需要从 sh_name 字段读取
        bool is_le = (ei_data == ELFDATA2LSB);

        if (is_64bit()) {
            uint64_t shoff = e_shoff + (&sec - sections.data()) * sizeof(Elf64_Shdr);
            Elf64_Shdr* shdr = (Elf64_Shdr*)(file_data.data() + shoff);
            name_offset = read_u32((uint8_t*)&shdr->sh_name, is_le);
        } else {
            uint64_t shoff = e_shoff + (&sec - sections.data()) * sizeof(Elf32_Shdr);
            Elf32_Shdr* shdr = (Elf32_Shdr*)(file_data.data() + shoff);
            name_offset = read_u32((uint8_t*)&shdr->sh_name, is_le);
        }

        if (name_offset < strtab.sh_size) {
            sec.name = strtab_data + name_offset;
        }
    }

    return true;
}

int ElfParser::find_segment_by_offset(uint64_t offset) const {
    for (int i = 0; i < (int)segments.size(); ++i) {
        if (offset >= segments[i].p_offset && offset < segments[i].get_end_offset()) {
            return i;
        }
    }
    return -1;
}

int ElfParser::find_section_by_offset(uint64_t offset) const {
    for (int i = 0; i < (int)sections.size(); ++i) {
        if (offset >= sections[i].sh_offset && offset < sections[i].get_end_offset()) {
            return i;
        }
    }
    return -1;
}
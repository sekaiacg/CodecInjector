#include "elf_symbols.h"

#include <elf.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

extern "C" {
#include "xz.h"
}

namespace sscport {
namespace {

struct Bytes {
    const uint8_t* data;
    size_t size;
};

const Elf64_Shdr* sections(Bytes elf, size_t* count) {
    if (elf.size < sizeof(Elf64_Ehdr) || memcmp(elf.data, ELFMAG, SELFMAG) != 0) return nullptr;
    const auto* header = reinterpret_cast<const Elf64_Ehdr*>(elf.data);
    if (header->e_ident[EI_CLASS] != ELFCLASS64 || header->e_shentsize != sizeof(Elf64_Shdr) ||
        header->e_shoff > elf.size ||
        header->e_shnum > (elf.size - header->e_shoff) / sizeof(Elf64_Shdr))
        return nullptr;
    *count = header->e_shnum;
    return reinterpret_cast<const Elf64_Shdr*>(elf.data + header->e_shoff);
}

bool section_bytes(Bytes elf, const Elf64_Shdr& section, Bytes* out) {
    if (section.sh_type == SHT_NOBITS || section.sh_offset > elf.size ||
        section.sh_size > elf.size - section.sh_offset)
        return false;
    *out = {elf.data + section.sh_offset, static_cast<size_t>(section.sh_size)};
    return true;
}

bool named_section(Bytes elf, const char* name, Bytes* out) {
    size_t count = 0;
    const Elf64_Shdr* table = sections(elf, &count);
    if (!table) return false;
    const auto* header = reinterpret_cast<const Elf64_Ehdr*>(elf.data);
    Bytes names{};
    if (header->e_shstrndx >= count || !section_bytes(elf, table[header->e_shstrndx], &names))
        return false;
    for (size_t i = 0; i < count; ++i) {
        size_t offset = table[i].sh_name;
        if (offset >= names.size) continue;
        const char* candidate = reinterpret_cast<const char*>(names.data + offset);
        if (strnlen(candidate, names.size - offset) < names.size - offset &&
            strcmp(candidate, name) == 0)
            return section_bytes(elf, table[i], out);
    }
    return false;
}

// Looks up every unresolved query in the SHT_SYMTAB and SHT_DYNSYM tables of one ELF image.
size_t search(Bytes elf, uintptr_t load_bias, SymbolQuery* queries, size_t query_count) {
    size_t count = 0, found = 0;
    const Elf64_Shdr* table = sections(elf, &count);
    if (!table) return 0;
    for (size_t i = 0; i < count; ++i) {
        if ((table[i].sh_type != SHT_SYMTAB && table[i].sh_type != SHT_DYNSYM) ||
            table[i].sh_entsize != sizeof(Elf64_Sym) || table[i].sh_link >= count)
            continue;
        Bytes symbols{}, strings{};
        if (!section_bytes(elf, table[i], &symbols) ||
            !section_bytes(elf, table[table[i].sh_link], &strings))
            continue;
        const auto* symbol = reinterpret_cast<const Elf64_Sym*>(symbols.data);
        for (size_t s = 0; s < symbols.size / sizeof(Elf64_Sym); ++s) {
            if (ELF64_ST_TYPE(symbol[s].st_info) != STT_FUNC || symbol[s].st_value == 0 ||
                symbol[s].st_name >= strings.size)
                continue;
            const char* name = reinterpret_cast<const char*>(strings.data + symbol[s].st_name);
            size_t limit = strings.size - symbol[s].st_name;
            for (size_t q = 0; q < query_count; ++q) {
                if (*queries[q].address || strncmp(name, queries[q].name, limit) != 0) continue;
                *queries[q].address = reinterpret_cast<void*>(load_bias + symbol[s].st_value);
                ++found;
            }
        }
    }
    return found;
}

// Decompresses an xz stream; returns a malloc'd buffer or nullptr.
uint8_t* unxz(Bytes input, size_t* output_size) {
    xz_crc32_init();
    xz_crc64_init();
    xz_dec* decoder = xz_dec_init(XZ_DYNALLOC, UINT32_C(1) << 26);
    if (!decoder) return nullptr;
    size_t capacity = input.size * 8 + 4096;
    auto* output = static_cast<uint8_t*>(malloc(capacity));
    xz_buf buffer{input.data, 0, input.size, output, 0, capacity};
    xz_ret result = XZ_OK;
    while (output && result == XZ_OK) {
        result = xz_dec_run(decoder, &buffer);
        if (result == XZ_OK && buffer.out_pos == buffer.out_size) {
            capacity *= 2;
            auto* grown = static_cast<uint8_t*>(realloc(output, capacity));
            if (!grown) {
                free(output);
                output = nullptr;
                break;
            }
            output = grown;
            buffer.out = output;
            buffer.out_size = capacity;
        }
    }
    xz_dec_end(decoder);
    if (result != XZ_STREAM_END) {
        free(output);
        return nullptr;
    }
    *output_size = buffer.out_pos;
    return output;
}

}  // namespace

size_t resolve_symbols(const char* path, uintptr_t load_bias, SymbolQuery* queries, size_t count) {
    for (size_t q = 0; q < count; ++q) *queries[q].address = nullptr;
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return 0;
    struct stat status {};
    void* mapping = MAP_FAILED;
    if (fstat(fd, &status) == 0 && status.st_size > 0)
        mapping = mmap(nullptr, status.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (mapping == MAP_FAILED) return 0;
    Bytes file{static_cast<const uint8_t*>(mapping), static_cast<size_t>(status.st_size)};

    size_t found = search(file, load_bias, queries, count);
    Bytes compressed{};
    if (found < count && named_section(file, ".gnu_debugdata", &compressed)) {
        size_t size = 0;
        uint8_t* inner = unxz(compressed, &size);
        if (inner) {
            found += search({inner, size}, load_bias, queries, count);
            free(inner);
        }
    }
    munmap(mapping, status.st_size);
    return found;
}

}  // namespace sscport

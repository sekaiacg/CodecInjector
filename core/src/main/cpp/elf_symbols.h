#pragma once

#include <stddef.h>
#include <stdint.h>

namespace port {

struct SymbolQuery {
    const char* name;
    void** address;
};

// Resolves function symbols of an already loaded ELF image by name. Android system libraries
// keep their full function symbol table in the xz-compressed MiniDebugInfo section
// (.gnu_debugdata); plain .symtab/.dynsym entries of the file itself are searched as well.
// Each found address is load_bias + st_value. Returns the number of queries that were resolved.
size_t resolve_symbols(const char* path, uintptr_t load_bias, SymbolQuery* queries, size_t count);

}  // namespace port

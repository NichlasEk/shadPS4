// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <common/va_ctx.h>
#include <array>
#include <atomic>
namespace Libraries::Kernel { extern const char* g_environment[64]; }
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/libs.h"
#include "libc_internal.h"
#include "libc_internal_io.h"
#include "libc_internal_math.h"
#include "libc_internal_memory.h"
#include "libc_internal_str.h"
#include "libc_internal_threads.h"
#include "printf.h"

namespace Libraries::LibcInternal {

static const char* PS4_SYSV_ABI mono_getenv(const char* name) {
    if (!name || !*name || std::strchr(name, '=')) return nullptr;
    const size_t length = std::strlen(name);
    for (const char* entry : Kernel::g_environment) {
        if (!entry) break;
        if (std::strncmp(entry, name, length) == 0 && entry[length] == '=') return entry + length + 1;
    }
    return nullptr;
}
using ConstraintHandler = void PS4_SYSV_ABI (*)(const char*, void*, int);
static std::atomic<ConstraintHandler> mono_constraint_handler{};
static ConstraintHandler PS4_SYSV_ABI mono_set_constraint_handler(ConstraintHandler handler) {
    return mono_constraint_handler.exchange(handler);
}

// ASCII C-locale classification. The guest uses Dinkumware bit positions
// (not the host libc table); index -1 is the EOF slot.
static const u16* PS4_SYSV_ABI mono_getpctype() {
    static constexpr auto table = [] {
        std::array<u16, 257> values{};
        for (unsigned c = 0; c < 128; ++c) {
            u16 mask = 0;
            if (c < 32 || c == 127) mask |= (c >= 9 && c <= 13) ? 0x40 : 0x80;
            if (c == 32) mask |= 4;
            if (c >= '0' && c <= '9') mask |= 0x21;
            if (c >= 'A' && c <= 'Z') mask |= 2;
            if (c >= 'a' && c <= 'z') mask |= 0x10;
            if ((c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) mask |= 1;
            if (c > 32 && c < 127 && !(mask & 0x32)) mask |= 8;
            values[c + 1] = mask;
        }
        return values;
    }();
    return table.data() + 1;
}

template<bool Upper>
static const u16* PS4_SYSV_ABI mono_case_table() {
    static constexpr auto table = [] {
        std::array<u16, 257> values{};
        values[0] = 0xffff;
        for (unsigned c=0; c<256; ++c) {
            values[c+1] = Upper ? (c >= 'a' && c <= 'z' ? c - 32 : c)
                                : (c >= 'A' && c <= 'Z' ? c + 32 : c);
        }
        return values;
    }();
    return table.data() + 1;
}
#ifdef __linux__
static void PS4_SYSV_ABI mono_qsort(void* base, size_t count, size_t width,
                                   int PS4_SYSV_ABI (*compare)(const void*, const void*)) {
    std::qsort(base, count, width, compare);
}
#endif

void RegisterLib(Core::Loader::SymbolsResolver* sym) {
    if (const char* enabled = std::getenv("SHADPS4_EXPERIMENTAL_MONO"); enabled && std::strcmp(enabled, "1") == 0) {
#ifdef __linux__
        LIB_FUNCTION("AEJdIVZTEmo", "libSceLibcInternal", 1, "libSceLibcInternal", mono_qsort);
#endif
        LIB_FUNCTION("1uJgoVq3bQU", "libSceLibcInternal", 1, "libSceLibcInternal", mono_case_table<false>);
        LIB_FUNCTION("rcQCUr0EaRU", "libSceLibcInternal", 1, "libSceLibcInternal", mono_case_table<true>);
        LIB_FUNCTION("smbQukfxYJM", "libSceLibcInternal", 1, "libSceLibcInternal", mono_getenv);
        LIB_FUNCTION("ENLfKJEZTjE", "libSceLibcInternal", 1, "libSceLibcInternal", mono_set_constraint_handler);
        LIB_FUNCTION("sUP1hBaouOw", "libSceLibcInternal", 1, "libSceLibcInternal", mono_getpctype);
    }
    RegisterlibSceLibcInternalMath(sym);
    RegisterlibSceLibcInternalStr(sym);
    RegisterlibSceLibcInternalMemory(sym);
    RegisterlibSceLibcInternalIo(sym);
    RegisterlibSceLibcInternalThreads(sym);
}

} // namespace Libraries::LibcInternal
// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/libs.h"
#include "libc_internal_memory.h"
#include "mono_mspace.h"

namespace Libraries::LibcInternal {

static MonoMspaces mono_mspaces;

static void* PS4_SYSV_ABI mono_mspace_create(const char* name, void* base, size_t capacity,
                                             unsigned flags) {
    void* handle = mono_mspaces.Create(base, capacity, flags);
    LOG_INFO(Lib_LibcInternal,
             "Experimental Mono mspace: name={} base={} size={:#x} flags={:#x} handle={}",
             name ? name : "(null)", base, capacity, flags, handle);
    return handle;
}
static void* PS4_SYSV_ABI mono_mspace_malloc(void* handle, size_t size) {
    auto space = mono_mspaces.Find(handle);
    return space ? space->Allocate(size) : nullptr;
}
static void* PS4_SYSV_ABI mono_mspace_calloc(void* handle, size_t count, size_t size) {
    auto space = mono_mspaces.Find(handle);
    return space ? space->Calloc(count, size) : nullptr;
}
static void* PS4_SYSV_ABI mono_mspace_realloc(void* handle, void* pointer, size_t size) {
    auto space = mono_mspaces.Find(handle);
    return space ? space->Reallocate(pointer, size) : nullptr;
}
static void PS4_SYSV_ABI mono_mspace_free(void* handle, void* pointer) {
    auto space = mono_mspaces.Find(handle);
    if (!space || !space->Free(pointer)) {
        LOG_ERROR(Lib_LibcInternal, "Invalid experimental mspace free: handle={} pointer={}",
                  handle, pointer);
    }
}

void* PS4_SYSV_ABI internal_memset(void* s, int c, size_t n) {
    return std::memset(s, c, n);
}

static void* PS4_SYSV_ABI mono_memmove(void* dst, const void* src, size_t count) {
    return std::memmove(dst, src, count);
}

void* PS4_SYSV_ABI internal_memcpy(void* dest, const void* src, size_t n) {
    return std::memcpy(dest, src, n);
}

s32 PS4_SYSV_ABI internal_memcpy_s(void* dest, size_t destsz, const void* src, size_t count) {
#ifdef _WIN64
    return memcpy_s(dest, destsz, src, count);
#else
    std::memcpy(dest, src, count);
    return 0; // ALL OK
#endif
}

s32 PS4_SYSV_ABI internal_memcmp(const void* s1, const void* s2, size_t n) {
    return std::memcmp(s1, s2, n);
}

static u64 g_mspace_atomic_id_mask = 0;
static u64 g_mstate_table[64] = {0};

struct HeapInfoInfo {
    u64 size = sizeof(HeapInfoInfo);
    u32 flag;
    u32 getSegmentInfo;
    u64* mspace_atomic_id_mask;
    u64* mstate_table;
};

void PS4_SYSV_ABI sceLibcHeapGetTraceInfo(HeapInfoInfo* info) {
    info->mspace_atomic_id_mask = &g_mspace_atomic_id_mask;
    info->mstate_table = g_mstate_table;
    info->getSegmentInfo = 0;
}

void RegisterlibSceLibcInternalMemory(Core::Loader::SymbolsResolver* sym) {

    if (const char* enabled = std::getenv("SHADPS4_EXPERIMENTAL_MONO");
        enabled && std::strcmp(enabled, "1") == 0) {
        LIB_FUNCTION("+P6FRGH4LfA", "libSceLibcInternal", 1, "libSceLibcInternal", mono_memmove);
        LIB_FUNCTION("-hn1tcVHq5Q", "libSceLibcInternal", 1, "libSceLibcInternal",
                     mono_mspace_create);
        LIB_FUNCTION("OJjm-QOIHlI", "libSceLibcInternal", 1, "libSceLibcInternal",
                     mono_mspace_malloc);
        LIB_FUNCTION("LYo3GhIlB38", "libSceLibcInternal", 1, "libSceLibcInternal",
                     mono_mspace_calloc);
        LIB_FUNCTION("gigoVHZvVPE", "libSceLibcInternal", 1, "libSceLibcInternal",
                     mono_mspace_realloc);
        LIB_FUNCTION("Vla-Z+eXlxo", "libSceLibcInternal", 1, "libSceLibcInternal",
                     mono_mspace_free);
    }

    LIB_FUNCTION("NFLs+dRJGNg", "libSceLibcInternal", 1, "libSceLibcInternal", internal_memcpy_s);
    LIB_FUNCTION("Q3VBxCXhUHs", "libSceLibcInternal", 1, "libSceLibcInternal", internal_memcpy);
    LIB_FUNCTION("8zTFvBIAIN8", "libSceLibcInternal", 1, "libSceLibcInternal", internal_memset);
    LIB_FUNCTION("DfivPArhucg", "libSceLibcInternal", 1, "libSceLibcInternal", internal_memcmp);

    LIB_FUNCTION("NWtTN10cJzE", "libSceLibcInternalExt", 1, "libSceLibcInternal",
                 sceLibcHeapGetTraceInfo);
}

} // namespace Libraries::LibcInternal

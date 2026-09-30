// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <unordered_map>
#ifdef __linux__
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace Libraries::LibcInternal {

// Experimental host-backed dynamic mspaces. Only the observed Mono mode
// (base=null, capacity=0, flags=6) is supported; not a fixed-arena allocator.
class MonoMspace {
public:
    ~MonoMspace() {
        for (const auto& [pointer, size] : allocations) {
            Release(pointer);
        }
#ifdef __linux__
        for (const auto& [pointer, backing] : guarded)
            ::munmap(backing.base, backing.size);
#endif
    }

    void* Allocate(size_t size, bool zero = false) {
        std::scoped_lock lock{mutex};
        return AllocateLocked(size, zero);
    }

    void* Calloc(size_t count, size_t size) {
        if (size && count > std::numeric_limits<size_t>::max() / size) {
            return nullptr;
        }
        return Allocate(count * size, true);
    }

    bool Free(void* pointer) {
        if (!pointer) {
            return true;
        }
        std::scoped_lock lock{mutex};
        const auto found = allocations.find(pointer);
        if (found == allocations.end()) {
            return false;
        }
        Release(pointer);
        allocations.erase(found);
        return true;
    }

    void* Reallocate(void* pointer, size_t size) {
        if (!pointer) {
            return Allocate(size);
        }
        std::scoped_lock lock{mutex};
        auto found = allocations.find(pointer);
        if (found == allocations.end()) {
            return nullptr;
        }
        if (size == 0) {
            Release(pointer);
            allocations.erase(found);
            return nullptr;
        }
        const size_t old_size = found->second;
        void* replacement = AllocateLocked(size, false);
        if (!replacement) {
            return nullptr; // Original allocation remains valid on failure.
        }
        std::memcpy(replacement, pointer, size < old_size ? size : old_size);
        allocations.erase(pointer); // AllocateLocked may have rehashed the map.
        Release(pointer);
        return replacement;
    }

private:
    void* AllocateLocked(size_t size, bool zero) {
        // PTRDIFF_MAX is also the host allocator's maximum object size.
        if (size > static_cast<size_t>(std::numeric_limits<ptrdiff_t>::max())) {
            return nullptr;
        }
        void* pointer = nullptr;
#ifdef __linux__
        if (guard_heap) {
            const size_t page = static_cast<size_t>(::sysconf(_SC_PAGESIZE));
            const size_t rounded = ((size ? size : 1) + 15) & ~size_t{15};
            const size_t writable = (rounded + page - 1) & ~(page - 1);
            const size_t total = writable + 2 * page;
            void* base = ::mmap(nullptr, total, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (base == MAP_FAILED)
                return nullptr;
            auto* start = static_cast<char*>(base) + page;
            if (::mprotect(start, writable, PROT_READ | PROT_WRITE) != 0) {
                ::munmap(base, total);
                return nullptr;
            }
            pointer = start + writable - rounded;
            try {
                guarded.emplace(pointer, GuardBacking{base, total});
            } catch (const std::bad_alloc&) {
                ::munmap(base, total);
                return nullptr;
            }
        } else
#endif
            pointer = zero ? std::calloc(1, size ? size : 1) : std::malloc(size ? size : 1);
        if (!pointer) {
            return nullptr;
        }
        try {
            allocations.emplace(pointer, size);
        } catch (const std::bad_alloc&) {
            Release(pointer);
            return nullptr;
        }
        return pointer;
    }
    void Release(void* pointer) {
#ifdef __linux__
        if (guard_heap) {
            const auto& block = guarded.at(pointer);
            // Keep the address reserved until pool destruction: catches guest
            // use-after-free without recycling it into another allocation.
            ::mprotect(block.base, block.size, PROT_NONE);
            return;
        }
#endif
        std::free(pointer);
    }
#ifdef __linux__
    const bool guard_heap = [] {
        const char* value = std::getenv("SHADPS4_MONO_GUARD_HEAP");
        return value && std::strcmp(value, "1") == 0;
    }();
    struct GuardBacking {
        void* base;
        size_t size;
    };
    std::unordered_map<void*, GuardBacking> guarded;
#endif
    std::mutex mutex;
    std::unordered_map<void*, size_t> allocations;
};

class MonoMspaces {
public:
    void* Create(void* base, size_t capacity, unsigned flags) {
        if (base || capacity || flags != 6) {
            return nullptr;
        }
        std::scoped_lock lock{mutex};
        try {
            auto space = std::make_shared<MonoMspace>();
            // Opaque handle is never dereferenced by the guest or the lookup.
            auto handle = reinterpret_cast<void*>(next++);
            spaces.emplace(handle, std::move(space));
            return handle;
        } catch (const std::bad_alloc&) {
            return nullptr;
        }
    }
    std::shared_ptr<MonoMspace> Find(void* handle) {
        std::scoped_lock lock{mutex};
        const auto found = spaces.find(handle);
        return found == spaces.end() ? nullptr : found->second;
    }
    bool Destroy(void* handle) {
        std::scoped_lock lock{mutex};
        return spaces.erase(handle) != 0;
    }

private:
    std::mutex mutex;
    std::unordered_map<void*, std::shared_ptr<MonoMspace>> spaces;
    uintptr_t next = 1;
};
} // namespace Libraries::LibcInternal

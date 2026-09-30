// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include <cassert>
#include <cstdio>
#include <thread>
#include <vector>
#include "core/libraries/libc_internal/mono_mspace.h"
using namespace Libraries::LibcInternal;
int main() {
    MonoMspaces registry;
    assert(!registry.Create(reinterpret_cast<void*>(16), 1024, 6));
    assert(!registry.Create(nullptr, 1024, 6));
    assert(!registry.Create(nullptr, 0, 0));
    void* handle = registry.Create(nullptr, 0, 6);
    auto space = registry.Find(handle);
    assert(space && !registry.Find(nullptr));
    auto other = registry.Find(registry.Create(nullptr, 0, 6));
    auto p = static_cast<unsigned char*>(space->Calloc(17, 13));
    assert(p && reinterpret_cast<uintptr_t>(p) % 16 == 0);
    for (int i = 0; i < 221; ++i) {
        assert(p[i] == 0);
        p[i] = static_cast<unsigned char>(i);
    }
    assert(!other->Free(p));
    assert(!space->Reallocate(p, SIZE_MAX));
    p = static_cast<unsigned char*>(space->Reallocate(p, 1024));
    assert(p);
    for (int i = 0; i < 221; ++i)
        assert(p[i] == static_cast<unsigned char>(i));
    p = static_cast<unsigned char*>(space->Reallocate(p, 32));
    for (int i = 0; i < 32; ++i)
        assert(p[i] == static_cast<unsigned char>(i));
    assert(!space->Reallocate(p, 0));
    assert(!space->Free(p));
    assert(space->Free(nullptr));
    assert(!space->Calloc(SIZE_MAX, 2));
    assert(!space->Allocate(SIZE_MAX));
    assert(space->Free(space->Reallocate(nullptr, 8)));
    assert(space->Free(space->Allocate(0)));
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t)
        threads.emplace_back([space] {
            for (int i = 0; i < 2000; ++i) {
                auto p = static_cast<unsigned char*>(space->Allocate(127));
                assert(p);
                std::memset(p, 0x5a, 127);
                auto q = static_cast<unsigned char*>(space->Reallocate(p, 257));
                assert(q);
                for (int j = 0; j < 127; ++j)
                    assert(q[j] == 0x5a);
                assert(space->Free(q));
            }
        });
    for (auto& thread : threads)
        thread.join();
    assert(registry.Destroy(handle));
    assert(!registry.Find(handle));
    assert(!registry.Destroy(handle));
    // Outstanding lookup keeps a pool alive during destruction of its handle.
    assert(space->Allocate(19)); // Released by pool destructor, checked by ASan.
    std::puts("PASS Mono mspace: overflow, realloc preservation, isolation, alignment, lifetime, "
              "threads");
}

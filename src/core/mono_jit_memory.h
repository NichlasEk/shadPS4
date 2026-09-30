// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#if defined(__linux__) && defined(__x86_64__)
#include <atomic>
#include <map>
#include <mutex>
#include <sys/mman.h>
#include <sys/stat.h>
#include <ucontext.h>
#include "common/decoder.h"
#include "common/logging/log.h"
#include "core/signals.h"
#include "core/tls.h"

namespace Core {
// Mono emits FS-based TCB accesses after ELF loading. Linux reserves FS for
// host libc, while shadPS4 keeps the guest TCB in GS. Track write/execute
// transitions on JIT backing pages so fresh code is translated before use.
class MonoJitMemory {
    struct Page {
        u64 device, inode, offset;
        int prot;
    };
    std::map<uintptr_t, Page> pages;
    std::mutex mutex;
    std::once_flag handler_once;
    std::atomic<uintptr_t> low{UINTPTR_MAX}, high{0};
    static constexpr size_t page_size = 4096;

    static bool SameBacking(const Page& a, const Page& b) {
        return a.device == b.device && a.inode == b.inode && a.offset == b.offset;
    }
    static void Protect(uintptr_t addr, int prot) {
        ASSERT_MSG(::mprotect(reinterpret_cast<void*>(addr), page_size, prot) == 0,
                   "Cannot protect experimental JIT page {:#x}", addr);
    }
    static size_t Patch(uintptr_t addr, size_t size, size_t readable_tail = 0) {
        size_t count = 0;
        for (size_t cursor = 0; cursor < size;) {
            auto* code = reinterpret_cast<u8*>(addr + cursor);
            if (*code != 0x64) {
                ++cursor;
                continue;
            }
            ZydisDecodedInstruction instruction{};
            ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT]{};
            if (!ZYAN_SUCCESS(Common::Decoder::Instance()->decodeInstruction(
                    instruction, operands, code, std::min<size_t>(15, size + readable_tail - cursor)))) {
                ++cursor;
                continue;
            }
            // Same TCB read shape as the existing ELF CPU patcher. Replacing
            // only the segment prefix preserves instruction length/addresses.
            if (instruction.mnemonic == ZYDIS_MNEMONIC_MOV &&
                operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER && operands[0].size == 64 &&
                operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY &&
                operands[1].mem.segment == ZYDIS_REGISTER_FS &&
                operands[1].mem.base == ZYDIS_REGISTER_NONE &&
                operands[1].mem.index == ZYDIS_REGISTER_NONE && operands[1].mem.disp.value >= 0 &&
                operands[1].mem.disp.value < sizeof(Tcb)) {
                for (unsigned i = 0; i < instruction.raw.prefix_count; ++i) {
                    if (instruction.raw.prefixes[i].value == 0x64) {
                        code[i] = 0x65;
                        ++count;
                    }
                }
            }
            // JIT pages mix code, padding and embedded data; a linear decode
            // from the page base can cross a later method's first instruction.
            // Inspect each possible instruction start for the narrow TCB shape.
            ++cursor;
        }
        return count;
    }

    bool Fault(void* raw, uintptr_t address) {
        if (address < low.load() || address >= high.load())
            return false;
        auto* ctx = static_cast<ucontext_t*>(raw);
        const auto error = ctx->uc_mcontext.gregs[REG_ERR];
        const bool execute = (error & 16) != 0, write = (error & 2) != 0;
        const uintptr_t base = address & ~(page_size - 1);
        std::scoped_lock lock{mutex};
        auto found = pages.find(base);
        if (found == pages.end())
            return false;
        const auto page = found->second;
        if (execute && (page.prot & PROT_EXEC)) {
            // Stop writes through every alias before translating this backing.
            for (const auto& [alias, other] : pages) {
                if (SameBacking(page, other))
                    Protect(alias, PROT_READ);
            }
            Protect(base, PROT_READ | PROT_WRITE);
            const auto next = pages.find(base + page_size);
            const size_t tail = next != pages.end() && (next->second.prot & PROT_READ) ? 14 : 0;
            size_t patched = Patch(base, page_size, tail);
            const auto pc = static_cast<uintptr_t>(ctx->uc_mcontext.gregs[REG_RIP]);
            if (pc > base && pc < base + page_size)
                patched += Patch(pc, base + page_size - pc, tail);
            if (pc < base && base - pc < 15) {
                const auto previous = pages.find(base - page_size);
                if (previous != pages.end() && (previous->second.prot & PROT_EXEC)) {
                    Protect(previous->first, PROT_READ | PROT_WRITE);
                    patched += Patch(pc, 15);
                    Protect(previous->first, PROT_READ | PROT_EXEC);
                }
            }
            Protect(base, PROT_READ | PROT_EXEC);
            if (patched)
                LOG_INFO(Core, "Mono JIT translated {} TLS reads at {:#x}", patched, base);
            return true;
        }
        if (write && (page.prot & PROT_WRITE)) {
            const auto pc = static_cast<uintptr_t>(ctx->uc_mcontext.gregs[REG_RIP]);
            if (pc >= base && pc < base + page_size) {
                LOG_ERROR(Core, "Self-modifying execution on one JIT page is unsupported");
                return false;
            }
            // Writes invalidate executable aliases too, including after the
            // guest has closed every descriptor for this shared backing.
            for (const auto& [alias, other] : pages) {
                if (SameBacking(page, other))
                    Protect(alias, other.prot & ~PROT_EXEC);
            }
            return true;
        }
        return false;
    }

public:
    static MonoJitMemory& Instance() {
        static MonoJitMemory instance;
        return instance;
    }
    static bool Handle(void* ctx, void* addr) {
        return Instance().Fault(ctx, reinterpret_cast<uintptr_t>(addr));
    }
    void Map(uintptr_t addr, size_t size, int fd, u64 offset, int prot) {
        struct stat info{};
        ASSERT_MSG(::fstat(fd, &info) == 0, "JIT backing descriptor unavailable");
        std::call_once(handler_once,
                       [] { Signals::Instance()->RegisterAccessViolationHandler(Handle, 17); });
        std::scoped_lock lock{mutex};
        low.store(std::min(low.load(), addr));
        high.store(std::max(high.load(), addr + size));
        for (size_t pos = 0; pos < size; pos += page_size) {
            pages[addr + pos] = Page{static_cast<u64>(info.st_dev), static_cast<u64>(info.st_ino),
                                     offset + pos, prot};
            Protect(addr + pos, prot & ~(PROT_EXEC | PROT_WRITE));
        }
    }
    void Unmap(uintptr_t addr, size_t size) {
        std::scoped_lock lock{mutex};
        auto begin = pages.lower_bound(addr), end = pages.lower_bound(addr + size);
        pages.erase(begin, end);
    }
};
} // namespace Core
#endif

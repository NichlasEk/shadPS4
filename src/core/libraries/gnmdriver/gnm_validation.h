// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include "gnm_error.h"

namespace Libraries::GnmDriver::Validation {
enum class Access { Read, GpuRead, GpuWrite };
struct Result {
    int code{};
    std::size_t word{};
    const char* reason = "ok";
    std::uint32_t buffer{};
    bool constant{};
    explicit operator bool() const {
        return code == 0;
    }
};
constexpr bool GpuRange(std::uint64_t address, std::uint64_t bytes) {
    constexpr std::uint64_t limit = 1ull << 40;
    return address != 0 && bytes != 0 && address < limit && bytes <= limit - address;
}

// A diagnostic preflight, not a simulation of GPU caches or asynchronous execution.
// Unknown opcodes and inherited state are deliberately not guessed at here.
template <typename Check>
Result Packets(std::span<const std::uint32_t> words, Check&& check) {
    std::uint32_t index_bytes = 0;
    std::uint64_t index_base = 0;
    for (std::size_t pos = 0; pos < words.size();) {
        const auto header = words[pos];
        const auto type = header >> 30;
        const auto length = type == 2 ? 1u : ((header >> 16) & 0x3fff) + 2;
        auto fail = [&](int code, const char* why) { return Result{code, pos, why}; };
        if (type == 1)
            return fail(ORBIS_GNM_ERROR_VALIDATION_BAD_OP_CODE, "reserved PM4 packet type 1");
        if (length > words.size() - pos)
            return fail(ORBIS_GNM_ERROR_VALIDATION_DCB, "packet extends beyond command buffer");
        if (type != 3) {
            pos += length;
            continue;
        }
        const auto op = (header >> 8) & 0xff;
        const auto* p = words.data() + pos;
        std::size_t minimum = 2;
        switch (op) {
        case 0x26:
            minimum = 3;
            break; // INDEX_BASE
        case 0x27:
            minimum = 6;
            break; // DRAW_INDEX_2
        case 0x2d:
            minimum = 3;
            break; // DRAW_INDEX_AUTO
        case 0x35:
            minimum = 5;
            break; // DRAW_INDEX_OFFSET_2
        case 0x37:
            minimum = 5;
            break; // WRITE_DATA (at least one value)
        case 0x3c:
            minimum = 7;
            break; // WAIT_REG_MEM
        case 0x33:
        case 0x3f:
            minimum = 4;
            break; // INDIRECT_BUFFER
        case 0x47:
            minimum = 6;
            break; // EVENT_WRITE_EOP
        case 0x58:
            minimum = 7;
            break; // ACQUIRE_MEM
        case 0x68:
        case 0x69:
        case 0x76:
        case 0x79:
            minimum = 3;
            break; // register offset AND at least one value
        }
        if (length < minimum)
            return fail(ORBIS_GNM_ERROR_VALIDATION_DCB, "PM4 payload too short for opcode");
        auto memory = [&](std::uint64_t address, std::uint64_t bytes, Access access) {
            // Check owns GPU address-width/permission policy, including the
            // emulator's explicitly owned VideoOut label allocation.
            return address && bytes && bytes <= UINT64_MAX - address &&
                   check(address, bytes, access);
        };
        if (op == 0x12 || op == 0x33 || op == 0x3f) {
            index_base = 0;
            index_bytes = 0; // nested commands/clear-state may change inherited state
        }
        if (op == 0x68 || op == 0x69 || op == 0x76 || op == 0x79) {
            if (op == 0x68) {
                index_base = 0;
                index_bytes = 0;
            }
            const std::uint32_t bank_size = op == 0x76 ? 0x400 : op == 0x68 ? 0xc00 : 0x1000;
            const auto offset = p[1] & 0xffff;
            if (offset >= bank_size || length - 2 > bank_size - offset)
                return fail(ORBIS_GNM_ERROR_VALIDATION_DCB, "register write crosses register bank");
        } else if (op == 0x2a) {
            if ((p[1] & 3) > 1)
                return fail(ORBIS_GNM_ERROR_VALIDATION_INDEX_SIZE, "reserved index element size");
            index_bytes = (p[1] & 1) ? 4 : 2;
        } else if (op == 0x26) {
            index_base = std::uint64_t(p[1]) | (std::uint64_t(p[2]) << 32);
        } else if (op == 0x27 || op == 0x35) {
            const auto count = p[op == 0x27 ? 4 : 3];
            const auto offset = op == 0x35 ? p[2] : 0;
            const auto maximum = p[1];
            if (count > maximum)
                return fail(ORBIS_GNM_ERROR_VALIDATION_INDEX_BUFFER,
                            "draw exceeds declared index capacity");
            const auto base =
                op == 0x27 ? std::uint64_t(p[2]) | (std::uint64_t(p[3]) << 32) : index_base;
            // OFFSET draws may inherit INDEX_BASE/TYPE from a previous submission.
            // Check only information actually available in this command buffer.
            if (count && (op == 0x27 || (index_base && index_bytes))) {
                const auto stride = index_bytes ? index_bytes : 2;
                const auto address = base + std::uint64_t(offset) * stride;
                if (address < base || address % stride ||
                    !memory(address, std::uint64_t(count) * stride, Access::GpuRead))
                    return fail(ORBIS_GNM_ERROR_VALIDATION_INDEX_BUFFER,
                                "unmapped, misaligned or non-GPU-readable indices");
            }
            // DRAW_INDEX_2 also updates the GPU's index base.
            if (op == 0x27)
                index_base = base;
        } else if (op == 0x47) {
            const auto select = p[3] >> 29;
            // DataSelect::None does not write memory; null is legal in that case.
            if (select >= 1 && select <= 4) {
                const auto address = std::uint64_t(p[2]) | (std::uint64_t(p[3] & 0xffff) << 32);
                const auto bytes = select == 1 ? 4u : 8u;
                if (address % bytes || !memory(address, bytes, Access::GpuWrite))
                    return fail(ORBIS_GNM_ERROR_VALIDATION_WRITE_EVENT_OP,
                                "EOP destination is not aligned GPU-writable memory");
            }
        } else if (op == 0x3c && (p[1] & 0x10)) {
            const auto address = std::uint64_t(p[2] & ~3u) | (std::uint64_t(p[3]) << 32);
            if (!memory(address, 4, Access::GpuRead))
                return fail(ORBIS_GNM_ERROR_VALIDATION_RESOURCE,
                            "WAIT_REG_MEM address is not GPU-readable");
        } else if (op == 0x37 && (((p[1] >> 8) & 0xf) == 5)) {
            const auto address = std::uint64_t(p[2]) | (std::uint64_t(p[3]) << 32);
            const auto bytes = (p[1] & (1u << 16)) ? 4u : (length - 4) * 4;
            if (address % 4 || !memory(address, bytes, Access::GpuWrite))
                return fail(ORBIS_GNM_ERROR_VALIDATION_RESOURCE,
                            "WRITE_DATA destination is not GPU-writable");
        }
        pos += length;
    }
    return {};
}

// Validate the entire batch before queuing anything or patching flip commands.
// Check must validate host accessibility before any pointer is dereferenced.
template <typename Check>
Result Submission(std::uint32_t count, const std::uint32_t* const* dcbs, const std::uint32_t* sizes,
                  const std::uint32_t* const* ccbs, const std::uint32_t* csizes, bool strict,
                  Check&& check) {
    auto readable = [&](const void* p, std::uint64_t bytes) {
        return p && check(reinterpret_cast<std::uintptr_t>(p), bytes, Access::Read);
    };
    auto invalid = [](const char* why) {
        return Result{ORBIS_GNM_ERROR_SUBMISSION_FAILED_INVALID_ARGUMENT, 0, why};
    };
    if (!count || !readable(dcbs, std::uint64_t(count) * sizeof(*dcbs)) ||
        !readable(sizes, std::uint64_t(count) * sizeof(*sizes)))
        return invalid("empty batch or unreadable DCB arrays");
    if ((ccbs && (!csizes || !readable(ccbs, std::uint64_t(count) * sizeof(*ccbs)))) ||
        (csizes && !readable(csizes, std::uint64_t(count) * sizeof(*csizes))))
        return invalid("unreadable or incomplete CCB arrays");
    for (std::uint32_t i = 0; i < count; ++i) {
        for (bool constant : {false, true}) {
            const auto size = constant ? (csizes ? csizes[i] : 0) : sizes[i];
            const auto buffer = constant ? (ccbs ? ccbs[i] : nullptr) : dcbs[i];
            if (constant && size == 0)
                continue;
            Result out;
            if (!size || size > 0x3ffffc || size % 4 ||
                reinterpret_cast<std::uintptr_t>(buffer) % 4 || !readable(buffer, size)) {
                out =
                    invalid("command buffer must be readable, dword-aligned and 4..0x3ffffc bytes");
            } else if (strict) {
                const auto address = reinterpret_cast<std::uintptr_t>(buffer);
                if (!GpuRange(address, size) || !check(address, size, Access::GpuRead))
                    out = invalid("command buffer is not in the 40-bit GPU-readable address space");
                else
                    out = Packets(std::span(buffer, size / 4), check);
            }
            if (!out) {
                out.buffer = i;
                out.constant = constant;
                return out;
            }
        }
    }
    return {};
}
} // namespace Libraries::GnmDriver::Validation

// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace Libraries::LibcInternal {
// Dinkumware/Annex K bounded copy. A count-limited prefix need not include
// the source terminator, but the destination always receives one on success.
inline int MonoStrncpyS(char* dst, size_t capacity, const char* src, size_t count) {
    constexpr int invalid = 22, range = 34, truncated = 80;
    if (!dst || !capacity || capacity > static_cast<size_t>(PTRDIFF_MAX))
        return invalid;
    if (!src) {
        dst[0] = 0;
        return invalid;
    }
    const bool truncate = count == std::numeric_limits<size_t>::max();
    const size_t limit = truncate ? capacity - 1 : (count < capacity ? count : capacity);
    size_t length = 0;
    while (length < limit && src[length])
        ++length;
    if (length == capacity) {
        dst[0] = 0;
        return range;
    }
    const auto d = reinterpret_cast<uintptr_t>(dst), s = reinterpret_cast<uintptr_t>(src);
    if ((d >= s && d - s <= length) || (s > d && s - d <= length)) {
        dst[0] = 0;
        return invalid;
    }
    const bool cut = truncate && length == limit && src[length] != 0;
    std::memcpy(dst, src, length);
    dst[length] = 0;
    return cut ? truncated : 0;
}
} // namespace Libraries::LibcInternal

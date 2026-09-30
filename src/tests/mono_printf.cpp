// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include "core/libraries/libc_internal/printf.h"

// Native SysV caller supplies both register and stack arguments. The emulator
// decodes its ABI layout explicitly, never passes guest va_list to host printf.
static int format(char* dst, size_t n, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    static_assert(sizeof(args) == sizeof(Common::VaList));
    Common::VaList guest{};
    std::memcpy(&guest, args, sizeof(guest));
    const int result = Libraries::LibcInternal::_vsnprintf(Libraries::LibcInternal::_out_buffer,
                                                           dst, fmt, &guest, n);
    va_end(args);
    return result;
}
int main() {
    char buf[256];
    assert(format(buf, sizeof(buf), "%s %d %u %ld %lld %zu %.2f", "hello", -42, 42u, 12345678901L,
                  23456789012LL, size_t{17}, 1.25) == 44);
    assert(!strcmp(buf, "hello -42 42 12345678901 23456789012 17 1.25"));
    assert(format(buf, sizeof(buf), "%d%d%d%d%d%d%d%d", 1, 2, 3, 4, 5, 6, 7, 8) == 8);
    assert(!strcmp(buf, "12345678"));
    char tiny[5] = {'X', 'X', 'X', 'X', 'X'};
    assert(format(tiny + 1, 3, "%s", "abcdef") == 6);
    assert(tiny[0] == 'X' && tiny[1] == 'a' && tiny[2] == 'b' && tiny[3] == 0 && tiny[4] == 'X');
    assert(format(nullptr, 0, "%s:%d", "hello", 42) == 8);
    assert(format(tiny, 0, "ignored") == 7 && tiny[0] == 'X');
    assert(format(tiny, 1, "ignored") == 7 && tiny[0] == 0);
    assert(format(buf, sizeof(buf), "%*.*s", 7, 3, "abcdef") == 7);
    assert(!strcmp(buf, "    abc"));
    puts("PASS Mono formatting: SysV registers/stack, mixed types, bounded writes, sizing");
}

// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include <cassert>
#include <cstdio>
#include "core/libraries/libc_internal/mono_safe_string.h"
using Libraries::LibcInternal::MonoStrncpyS;
int main() {
    char bounded[5] = {'X', 'X', 'X', 'X', 'X'};
    assert(MonoStrncpyS(bounded + 1, 3, "abcdef", 2) == 0);
    assert(bounded[0] == 'X' && bounded[4] == 'X' && !std::strcmp(bounded + 1, "ab"));
    assert(MonoStrncpyS(bounded + 1, 3, "abcdef", 6) == 34);
    assert(bounded[1] == 0 && bounded[0] == 'X' && bounded[4] == 'X');
    assert(MonoStrncpyS(bounded + 1, 3, "a", 6) == 0 && !std::strcmp(bounded + 1, "a"));
    assert(MonoStrncpyS(bounded + 1, 3, "abc", 0) == 0 && bounded[1] == 0);
    assert(MonoStrncpyS(bounded + 1, 3, "abcdef", SIZE_MAX) == 80);
    assert(!std::strcmp(bounded + 1, "ab") && bounded[4] == 'X');
    char no_terminator[2] = {'a', 'b'};
    assert(MonoStrncpyS(bounded + 1, 3, no_terminator, 2) == 0);
    assert(MonoStrncpyS(nullptr, 3, "a", 1) == 22);
    assert(MonoStrncpyS(bounded, 0, "a", 1) == 22 && bounded[0] == 'X');
    assert(MonoStrncpyS(bounded, 5, nullptr, 1) == 22 && bounded[0] == 0);
    char overlap[8] = "abcdef";
    assert(MonoStrncpyS(overlap + 1, 7, overlap, 4) == 22 && overlap[1] == 0);
    puts("PASS Mono strncpy_s: bounded prefix, nonterminated source, truncation, errors, overlap");
}

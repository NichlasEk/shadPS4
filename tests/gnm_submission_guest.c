// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// OpenOrbis guest integration probe. Requires SHADPS4_GNM_VALIDATION=1.
#include <orbis/GnmDriver.h>
#include <orbis/libkernel.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static FILE* log_file;
static int failures;
static void expect(const char* name, int actual, uint32_t expected) {
    char text[256];
    snprintf(text, sizeof text, "GNM-PROBE %s %s got=%08x expected=%08x\n",
             (uint32_t)actual == expected ? "PASS" : "FAIL", name, (uint32_t)actual, expected);
    sceKernelDebugOutText(0, text);
    if (log_file) {
        fputs(text, log_file);
        fflush(log_file);
    }
    failures += (uint32_t)actual != expected;
}
int main(void) {
    log_file = fopen("/data/gnm-validation-probe.log", "w");
    off_t offset = 0;
    void* memory = 0;
    if (sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), 0x4000, 0x4000, 3,
                                      &offset) ||
        sceKernelMapDirectMemory(&memory, 0x4000, 0x33, 0, offset, 0x4000))
        return 2;
    uint32_t* p = memory;
    void* dcbs[2] = {p, p + 32};
    uint32_t sizes[2] = {8, 8};
    p[0] = 0xc0001000;
    p[1] = 0;
    expect("empty-submit", sceGnmSubmitCommandBuffers(0, 0, 0, 0, 0), 0x80d11000);
    expect("null-arrays", sceGnmSubmitCommandBuffers(1, 0, 0, 0, 0), 0x80d11000);
    sizes[0] = 7;
    expect("unaligned-size", sceGnmSubmitCommandBuffers(1, dcbs, sizes, 0, 0), 0x80d11000);
    sizes[0] = 8;
    dcbs[0] = (char*)memory + 2;
    expect("unaligned-base", sceGnmSubmitCommandBuffers(1, dcbs, sizes, 0, 0), 0x80d11000);
    dcbs[0] = 0;
    expect("null-buffer", sceGnmSubmitCommandBuffers(1, dcbs, sizes, 0, 0), 0x80d11000);
    dcbs[0] = p;
    p[0] = 0xc0006900;
    p[1] = 0x191;
    expect("physical-empty-ps-input", sceGnmSubmitCommandBuffers(1, dcbs, sizes, 0, 0), 0x80d13013);
    p[0] = 0xc0055800;
    expect("truncated-acquire", sceGnmSubmitCommandBuffers(1, dcbs, sizes, 0, 0), 0x80d13013);
    const uint32_t eop[] = {0xc0044700, 0x514, 0, 0x22000000, 1, 0};
    memcpy(p, eop, sizeof eop);
    sizes[0] = sizeof eop;
    expect("zero-eop-write", sceGnmSubmitCommandBuffers(1, dcbs, sizes, 0, 0), 0x80d13005);
    const uint32_t index[] = {0xc0002a00, 0x81, 0xc0042700, 3, 0, 0, 3, 0};
    memcpy(p, index, sizeof index);
    sizes[0] = sizeof index;
    expect("zero-index-address", sceGnmSubmitCommandBuffers(1, dcbs, sizes, 0, 0), 0x80d13006);
    p[0] = 0xc0001000;
    p[1] = 0;
    sizes[0] = 8;
    expect("empty-flip-submit", sceGnmSubmitAndFlipCommandBuffers(0, 0, 0, 0, 0, 1, 0, 0, 0),
           0x80d11000);
    expect("short-flip-submit", sceGnmSubmitAndFlipCommandBuffers(1, dcbs, sizes, 0, 0, 1, 0, 0, 0),
           0x80d11080);
    // A bad second DCB must prevent writes in the first DCB from ever being queued.
    uintptr_t label = (uintptr_t)(p + 1024);
    p[1024] = 0;
    uint32_t write[] = {0xc0033700, 0x500, (uint32_t)label, (uint32_t)(label >> 32), 0xabcdef};
    memcpy(p, write, sizeof write);
    sizes[0] = sizeof write;
    p[32] = 0xc0006900;
    p[33] = 0x191;
    expect("batch-preflight", sceGnmSubmitCommandBuffers(2, dcbs, sizes, 0, 0), 0x80d13013);
    sceKernelUsleep(10000);
    expect("batch-no-side-effects", p[1024], 0);
    p[0] = 0xc0001000;
    p[1] = 0;
    sizes[0] = 8;
    expect("valid-nop", sceGnmSubmitCommandBuffers(1, dcbs, sizes, 0, 0), 0);
    sceGnmSubmitDone();
    sceKernelUsleep(10000);
    expect("summary", failures, 0);
    if (log_file)
        fclose(log_file);
    return failures ? 1 : 0;
}

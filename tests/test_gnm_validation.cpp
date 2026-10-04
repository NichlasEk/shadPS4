// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstdlib>
#include <iostream>
#include <vector>
#include "core/libraries/gnmdriver/gnm_validation.h"
using namespace Libraries::GnmDriver::Validation;
static void require(bool value) {
    if (!value)
        std::abort();
}
static bool gpu(std::uint64_t p, std::uint64_t n, Access) {
    return p >= 0x200000000ull && p < 0x200010000ull && n <= 0x200010000ull - p;
}
static auto packet(std::initializer_list<std::uint32_t> words) {
    return Packets(std::span(words.begin(), words.size()), gpu);
}
int main() {
    require(!GpuRange(0, 4));
    require(GpuRange((1ull << 40) - 4, 4));
    require(!GpuRange((1ull << 40) - 4, 5));
    require(!GpuRange(UINT64_MAX - 3, 8));
    // Exact empty SPI_PS_INPUT_CNTL packet captured from physical UT99 0.29.
    auto r = packet({0xc0006900, 0x191});
    require(r.code == ORBIS_GNM_ERROR_VALIDATION_DCB && r.word == 0);
    require(bool(packet({0xc0016900, 0x191, 0x400})));
    require(packet({0xc0026900, 0xfff, 0, 0}).code == ORBIS_GNM_ERROR_VALIDATION_DCB);
    require(packet({0xc0016900, 0x191}).code == ORBIS_GNM_ERROR_VALIDATION_DCB);
    require(packet({0x40000000, 0}).code == ORBIS_GNM_ERROR_VALIDATION_BAD_OP_CODE);
    require(bool(packet({0x80000000, 0xc0001000, 0})));
    // Actual emitter-sized OFFSET packet: max_size is count, not offset+count.
    require(bool(packet({0xc0012600, 0, 2, 0xc0002a00, 0x81, 0xc0033500, 3, 12, 3, 0})));
    require(packet({0xc0033500, 2, 0, 3, 0}).code == ORBIS_GNM_ERROR_VALIDATION_INDEX_BUFFER);
    require(packet({0xc0002a00, 2}).code == ORBIS_GNM_ERROR_VALIDATION_INDEX_SIZE);
    require(bool(packet({0xc0002a00, 0x81, 0xc0042700, 3, 0, 2, 3, 0})));
    require(packet({0xc0002a00, 0x81, 0xc0042700, 3, 2, 2, 3, 0}).code ==
            ORBIS_GNM_ERROR_VALIDATION_INDEX_BUFFER);
    require(packet({0xc0042700, 3, 0xfffe, 2, 3, 0}).code ==
            ORBIS_GNM_ERROR_VALIDATION_INDEX_BUFFER);
    // No-data EOP at address zero is legal. An actual write there is not.
    require(bool(packet({0xc0044700, 0x514, 0, 0x01000000, 0, 0})));
    require(packet({0xc0044700, 0x514, 0, 0x22000000, 1, 0}).code ==
            ORBIS_GNM_ERROR_VALIDATION_WRITE_EVENT_OP);
    require(bool(packet({0xc0044700, 0x514, 0, 0x22000002, 1, 0})));
    require(packet({0xc0044700, 0x514, 4, 0x42000002, 1, 0}).code ==
            ORBIS_GNM_ERROR_VALIDATION_WRITE_EVENT_OP);
    require(packet({0xc0033700, 0x500, 0, 0, 0}).code == ORBIS_GNM_ERROR_VALIDATION_RESOURCE);
    require(bool(packet({0xc0033700, 0x500, 0, 2, 0})));
    require(packet({0xc0053c00, 0x13, 0, 0, 1, ~0u, 10}).code ==
            ORBIS_GNM_ERROR_VALIDATION_RESOURCE);
    require(bool(packet({0xc0053c00, 0x13, 0, 2, 1, ~0u, 10})));
    require(bool(packet({0xc0055800, 0x08c00000, ~0u, 0xff, 0, 0, 10})));
    // Malformed later packets report their precise offset.
    r = packet({0xc0001000, 0, 0xc0006900, 0x191});
    require(r.word == 2 && !r);
    // Deliberately unmapped pointers must be rejected WITHOUT touching memory.
    auto deny = [](std::uint64_t, std::uint64_t, Access) { return false; };
    require(!Submission(0, nullptr, nullptr, nullptr, nullptr, false, deny));
    require(!Submission(1, reinterpret_cast<const std::uint32_t* const*>(1), nullptr, nullptr,
                        nullptr, false, deny));
    std::uint32_t words[] = {0xc0001000, 0};
    const std::uint32_t* buffers[] = {words, words};
    std::uint32_t sizes[] = {8, 8};
    auto host = [](std::uint64_t p, std::uint64_t n, Access) { return p && n; };
    require(bool(Submission(2, buffers, sizes, nullptr, nullptr, false, host)));
    sizes[1] = 7;
    r = Submission(2, buffers, sizes, nullptr, nullptr, false, host);
    require(!r && r.buffer == 1 && r.code == ORBIS_GNM_ERROR_SUBMISSION_FAILED_INVALID_ARGUMENT);
    sizes[1] = 0x400000;
    require(!Submission(2, buffers, sizes, nullptr, nullptr, false, host));
    sizes[1] = 8;
    buffers[1] = nullptr;
    require(!Submission(2, buffers, sizes, nullptr, nullptr, false, host));
    require(!Submission(1, buffers, sizes, buffers, nullptr, false, host));
    std::uint32_t zero = 0;
    require(bool(Submission(1, buffers, sizes, nullptr, &zero, false, host)));
    // Every truncation of a known multiword packet is rejected before payload reads.
    const std::uint32_t eop[] = {0xc0044700, 0x514, 0, 0x22000002, 1, 0};
    for (std::size_t n = 1; n < 6; ++n)
        require(!Packets(std::span(eop, n), gpu));
    std::cout << "PASS: PM4 bounds, empty registers, index ranges, zero labels, permissions "
                 "callback, submission arrays\n";
}

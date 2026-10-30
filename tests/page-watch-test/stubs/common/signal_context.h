// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdlib>
namespace Common { inline bool IsWriteError(void*) { std::abort(); } }

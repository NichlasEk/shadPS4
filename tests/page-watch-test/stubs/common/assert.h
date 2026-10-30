// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdexcept>
#define ASSERT_MSG(x, ...) do { if(!(x)) throw std::runtime_error("page watcher invariant"); } while(0)

#define ASSERT(x) ASSERT_MSG(x)

// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <boost/icl/interval_set.hpp>
#include <mutex>
#include "common/types.h"
namespace Vulkan {
// Only the mapping registry is a test double. PageManager itself is unmodified.
struct Rasterizer {
 boost::icl::interval_set<VAddr> mapped;
 std::recursive_mutex mutex;
 bool IsMapped(VAddr a,u64 n){std::lock_guard lock(mutex);return boost::icl::contains(mapped,decltype(mapped)::interval_type::right_open(a,a+n));}
 template<class F> void ForEachMappedRangeInRange(VAddr a,u64 n,F&& f){
  std::lock_guard lock(mutex);
  for(const auto& r:(mapped & decltype(mapped)::interval_type::right_open(a,a+n)))f(r);
 }
 void Map(VAddr a,u64 n){std::lock_guard lock(mutex);mapped+=decltype(mapped)::interval_type::right_open(a,a+n);}
 bool InvalidateMemory(VAddr,u64){std::abort();}
 bool ReadMemory(VAddr,u64){std::abort();}
};
}

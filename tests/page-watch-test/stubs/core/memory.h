// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <limits>
#include <sys/mman.h>
#include <system_error>
#include "common/types.h"
namespace Core {
enum class MemoryPermission {None=0, Read=1, Write=2, Execute=4};
inline MemoryPermission operator|(MemoryPermission a,MemoryPermission b){return MemoryPermission(int(a)|int(b));}
struct AddressSpace {
 unsigned calls=0;
 void Protect(VAddr a,u64 n,MemoryPermission p){
  ++calls;
  if(::mprotect(reinterpret_cast<void*>(a),n,int(p))) throw std::system_error(errno,std::generic_category(),"mprotect");
 }
};
struct Memory {AddressSpace space;static Memory* Instance(){static Memory v;return &v;}AddressSpace& GetAddressSpace(){return space;}};
}

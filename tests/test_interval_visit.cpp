// SPDX-License-Identifier: GPL-2.0-or-later
// Compile with -I<patched shadPS4>/src; tests the actual helper in the patch.
#include "common/interval_visit.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <random>
#include <vector>
static size_t allocations;
void* operator new(size_t n) {++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,size_t) noexcept {std::free(p);}
using Set=boost::icl::interval_set<uint64_t>;
using Interval=Set::interval_type;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL line%d\n",__LINE__);return 1;}}while(0)
int main(){
 std::mt19937_64 rng(8173);size_t queries=0;
 for(unsigned trial=0;trial<100;++trial){
  Set ranges;for(unsigned i=0;i<128;++i){auto lo=rng()%4096; ranges.add(Interval::right_open(lo,lo+rng()%32));}
  for(unsigned j=0;j<256;++j){
   auto lo=rng()%4200;auto query=Interval::right_open(lo,lo+rng()%100);
   const auto expected=ranges&query;
   std::vector<Interval> actual;actual.reserve(ranges.iterative_size());
   auto before=allocations;
   Common::VisitIntervalIntersections(ranges,query,[&](auto r){actual.push_back(r);});
   CHECK(allocations==before);CHECK(actual.size()==expected.iterative_size());
   auto it=expected.begin();for(auto r:actual){CHECK(r==*it);++it;}++queries;
  }
 }
 Set ranges;for(auto r:{Interval::right_open(0,10),Interval::right_open(20,30),Interval::right_open(UINT64_MAX-8,UINT64_MAX)})ranges.add(r);
 for(auto q:{Interval::right_open(10,20),Interval::right_open(0,0),Interval::right_open(5,25),Interval::right_open(UINT64_MAX-4,UINT64_MAX)}){
  Set actual;Common::VisitIntervalIntersections(ranges,q,[&](auto r){actual.add(r);});CHECK(actual==(ranges&q));++queries;
 }
 ranges.clear();for(uint64_t i=0;i<4096;++i)ranges.add(Interval::right_open(i*16,i*16+8));
 uint64_t sums[2]={};size_t counts[2]={};double times[2]={};
 for(unsigned mode=0;mode<2;++mode){auto before=allocations;auto start=std::chrono::steady_clock::now();
  for(uint64_t i=0;i<2000;++i){auto q=Interval::right_open((i%4096)*16+2,(i%4096)*16+6);
   if(mode)Common::VisitIntervalIntersections(ranges,q,[&](auto r){sums[mode]+=boost::icl::upper(r)-boost::icl::lower(r);});
   else for(auto r:(ranges&q))sums[mode]+=boost::icl::upper(r)-boost::icl::lower(r);
  }
  times[mode]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();counts[mode]=allocations-before;
 }
 CHECK(sums[0]==sums[1]&&sums[1]==8000);CHECK(counts[0]>0&&counts[1]==0);
 std::printf("PASS queries=%zu boundaries/gaps/empty/high-address exact equivalence; allocations old=%zu new=%zu ms old=%.3f new=%.3f\n",queries,counts[0],counts[1],times[0],times[1]);
}

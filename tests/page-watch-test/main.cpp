// SPDX-License-Identifier: GPL-2.0-or-later
// Compile the real verifier page_manager.cpp with dependency adapters; use real
// kernel mappings/protection faults. No Roblox code, GPU driver, or guest process.
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include "core/memory.h"
#include "video_core/page_manager.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"FAIL line %d errno=%d\n",__LINE__,errno);return 1;} }while(0)
static bool writable(void* p){
 pid_t child=fork();if(child<0)std::abort();
 if(!child){*static_cast<volatile unsigned char*>(p)=0x42;_exit(0);}
 int status=0;while(waitpid(child,&status,0)<0)if(errno!=EINTR)std::abort();
 if(WIFEXITED(status)&&WEXITSTATUS(status)==0)return true;
 if(WIFSIGNALED(status)&&(WTERMSIG(status)==SIGSEGV||WTERMSIG(status)==SIGBUS))return false;
 std::abort();
}
int main(int argc,char** argv){
 rlimit no_core{0,0};setrlimit(RLIMIT_CORE,&no_core);
 const size_t page=sysconf(_SC_PAGESIZE);CHECK(page==4096);
 void* memory=mmap(nullptr,3*page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
 CHECK(memory!=MAP_FAILED);VAddr a=reinterpret_cast<VAddr>(memory);
 char name[]="/tmp/cordial-page-watch-XXXXXX";int fd=mkstemp(name);CHECK(fd>=0);
 CHECK(!ftruncate(fd,page));const unsigned char marker=0x5a;CHECK(pwrite(fd,&marker,1,0)==1);CHECK(!close(fd));
 fd=open(name,O_RDONLY);CHECK(fd>=0);CHECK(!unlink(name));
 void* middle=mmap(reinterpret_cast<void*>(a+page),page,PROT_READ,MAP_SHARED|MAP_FIXED,fd,0);
 CHECK(middle==reinterpret_cast<void*>(a+page));CHECK(!close(fd));
 Vulkan::Rasterizer rasterizer;rasterizer.Map(a,page);rasterizer.Map(a+2*page,page);
 VideoCore::PageManager tracker(&rasterizer);
 tracker.OnGpuMap(a,page);tracker.OnGpuMap(a+2*page,page);
 CHECK(writable(memory)&&!writable(middle)&&writable(reinterpret_cast<void*>(a+2*page)));
 tracker.UpdatePageWatchers<true>(a,3*page);
 tracker.UpdatePageWatchers<true>(a,3*page);
 CHECK(!writable(memory)&&!writable(middle)&&!writable(reinterpret_cast<void*>(a+2*page)));
 tracker.UpdatePageWatchers<false>(a,3*page);
 CHECK(!writable(memory)&&!writable(reinterpret_cast<void*>(a+2*page)));
 try {tracker.UpdatePageWatchers<false>(a,3*page);}
 catch(const std::system_error& e){
  if(argc==2&&!std::strcmp(argv[1],"--expect-eacces")&&e.code().value()==EACCES){
   std::puts("CONTROL: real PageManager attempted writable protection on O_RDONLY shared file (EACCES)");return 0;
  }
  std::fprintf(stderr,"FAIL protection: %s\n",e.what());return 1;
 }
 CHECK(argc==1); // The old-code control must actually reject, not silently pass.
 CHECK(writable(memory)&&!writable(middle)&&writable(reinterpret_cast<void*>(a+2*page)));
 CHECK(*static_cast<volatile unsigned char*>(middle)==marker);
 CHECK(!munmap(memory,3*page));
 std::puts("PASS real PageManager nested watchers restore both GPU intervals and preserve readonly hole");
}

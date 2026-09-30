// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Linux bring-up backend: memfd gives real shared backing and OS lifetime
// across descriptor closure. Core::Memory supplies guest VMAs and protections.
#ifdef __linux__
#include <sys/mman.h>
#include <unistd.h>
#include "core/file_sys/backends/host_fs.h"
#include "core/file_sys/fs.h"

namespace Libraries::Kernel {

static u32 MonoJitProtection(u32 prot) {
    return prot | ((prot & 2) ? 1u : 0u);
}

static s32 MonoJitDescriptor(int host_fd, u32 prot, s32* output) {
    using namespace Core::FileSys;
    auto backing = std::make_unique<HostFile>(fmt::format("/proc/self/fd/{}", host_fd),
                                              Common::FS::FileAccessMode::ReadWrite, false);
    if (!backing->IsOpen()) {
        return ORBIS_KERNEL_ERROR_ENOMEM;
    }
    auto* handles = Common::Singleton<HandleTable>::Instance();
    const s32 fd = handles->CreateHandle();
    auto* file = handles->GetFile(fd);
    file->type = FileType::Regular;
    file->handle = std::move(backing);
    file->m_guest_name = "[experimental Mono JIT]";
    file->mono_jit = true;
    file->mono_jit_protection = MonoJitProtection(prot);
    file->is_opened = true;
    *output = fd;
    return ORBIS_OK;
}

static s32 PS4_SYSV_ABI mono_jit_create(const char* name, u64 size, s32 prot, s32* output) {
    if (!output)
        return ORBIS_KERNEL_ERROR_EFAULT;
    *output = -1;
    if (!size || (size & 0x3fff) || size > INT64_MAX || prot <= 0 || (prot & ~7)) {
        return ORBIS_KERNEL_ERROR_EINVAL;
    }
    const int backing = ::memfd_create("shadps4-mono-jit", MFD_CLOEXEC);
    if (backing < 0)
        return ORBIS_KERNEL_ERROR_ENOMEM;
    s32 result = ORBIS_KERNEL_ERROR_ENOMEM;
    if (::ftruncate(backing, static_cast<off_t>(size)) == 0) {
        result = MonoJitDescriptor(backing, prot, output);
    }
    ::close(backing);
    LOG_INFO(Kernel_Vmm, "Experimental JIT create: name={} size={:#x} prot={} fd={} result={}",
             name ? name : "(null)", size, prot, *output, result);
    return result;
}

static s32 PS4_SYSV_ABI mono_jit_alias(s32 fd, s32 prot, s32* output) {
    if (!output)
        return ORBIS_KERNEL_ERROR_EFAULT;
    *output = -1;
    auto* file = Common::Singleton<Core::FileSys::HandleTable>::Instance()->GetFile(fd);
    if (!file || !file->mono_jit)
        return ORBIS_KERNEL_ERROR_EBADF;
    if (prot <= 0 || (prot & ~7))
        return ORBIS_KERNEL_ERROR_EINVAL;
    if ((MonoJitProtection(prot) & ~file->mono_jit_protection) != 0) {
        return ORBIS_KERNEL_ERROR_EACCES;
    }
    const int backing = static_cast<int>(file->GetHostFile()->GetFileMapping());
    const s32 result = MonoJitDescriptor(backing, prot, output);
    LOG_INFO(Kernel_Vmm, "Experimental JIT alias: fd={} prot={} alias={} result={}", fd, prot,
             *output, result);
    return result;
}

static s32 PS4_SYSV_ABI mono_jit_map(s32 fd, s32 prot, void** output) {
    if (!output)
        return ORBIS_KERNEL_ERROR_EFAULT;
    *output = nullptr;
    auto* file = Common::Singleton<Core::FileSys::HandleTable>::Instance()->GetFile(fd);
    if (!file || !file->mono_jit)
        return ORBIS_KERNEL_ERROR_EBADF;
    if (prot <= 0 || (prot & ~7))
        return ORBIS_KERNEL_ERROR_EINVAL;
    const auto result = Core::Memory::Instance()->MapFile(output, 0, file->GetSize(),
                                                          static_cast<Core::MemoryProt>(prot),
                                                          Core::MemoryMapFlags::Shared, fd, 0);
    LOG_INFO(Kernel_Vmm, "Experimental JIT map: fd={} prot={} address={} result={}", fd, prot,
             *output, result);
    return result;
}
} // namespace Libraries::Kernel
#endif

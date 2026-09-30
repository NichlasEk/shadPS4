// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#if defined(__linux__) && defined(__x86_64__)
#include <linux/futex.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include <unistd.h>

namespace Libraries::Kernel {
// Emulator-private signal, excluded from guest signal masks and number mapping.
// The signal handler never allocates, logs, or takes a host mutex.
static void MonoWake(std::atomic<int>& state) {
    syscall(SYS_futex, &state, FUTEX_WAKE_PRIVATE, 1, nullptr, nullptr, 0);
}
static void MonoWait(std::atomic<int>& state, int value) {
    const timespec timeout{0, 10000000};
    syscall(SYS_futex, &state, FUTEX_WAIT_PRIVATE, value, &timeout, nullptr, 0);
}
static_assert(std::atomic<int>::is_always_lock_free);
static void MonoSuspendHandler(int, siginfo_t*, void* raw) {
    const int saved_errno = errno;
    auto* thread = g_curthread;
    if (!thread)
        return;
    int expected = 1;
    if (!thread->mono_suspend_state.compare_exchange_strong(expected, -1))
        return;
    thread->mono_suspend_context = raw;
    thread->mono_suspend_state.store(2, std::memory_order_release);
    MonoWake(thread->mono_suspend_state);
    while (thread->mono_suspend_state.load(std::memory_order_acquire) == 2)
        MonoWait(thread->mono_suspend_state, 2);
    thread->mono_suspend_context = nullptr;
    thread->mono_suspend_state.store(0, std::memory_order_release);
    MonoWake(thread->mono_suspend_state);
    errno = saved_errno;
}
static int PS4_SYSV_ABI mono_suspend_user_context(PthreadT thread) {
    if (!thread || !thread->native_thr)
        return POSIX_ESRCH;
    if (thread == g_curthread)
        return POSIX_EDEADLK;
    int expected = 0;
    if (!thread->mono_suspend_state.compare_exchange_strong(expected, 1))
        return POSIX_EBUSY;
    int result =
        pthread_kill(static_cast<pthread_t>(thread->native_thr->GetHandle()), SIGRTMAX - 1);
    if (result) {
        thread->mono_suspend_state.store(0);
        return POSIX_ESRCH;
    }
    for (unsigned waits = 0; thread->mono_suspend_state.load(std::memory_order_acquire) != 2;
         ++waits) {
        if (waits >= 500) {
            expected = 1;
            if (thread->mono_suspend_state.compare_exchange_strong(expected, 0))
                return POSIX_EAGAIN;
        }
        MonoWait(thread->mono_suspend_state, thread->mono_suspend_state.load());
    }
    return 0;
}
static int PS4_SYSV_ABI mono_get_user_context(PthreadT thread, Ucontext* out) {
    if (!thread || !out)
        return POSIX_EINVAL;
    if (thread->mono_suspend_state.load(std::memory_order_acquire) != 2)
        return POSIX_EINVAL;
    const auto* native = static_cast<const ucontext_t*>(thread->mono_suspend_context);
    if (!native)
        return POSIX_EINVAL;
    *out = {};
    auto& mc = out->uc_mcontext;
    const auto& r = native->uc_mcontext.gregs;
#define MONO_REG(name, reg) mc.mc_##name = r[REG_##reg]
    MONO_REG(r8, R8);
    MONO_REG(r9, R9);
    MONO_REG(r10, R10);
    MONO_REG(r11, R11);
    MONO_REG(r12, R12);
    MONO_REG(r13, R13);
    MONO_REG(r14, R14);
    MONO_REG(r15, R15);
    MONO_REG(rdi, RDI);
    MONO_REG(rsi, RSI);
    MONO_REG(rbp, RBP);
    MONO_REG(rbx, RBX);
    MONO_REG(rdx, RDX);
    MONO_REG(rax, RAX);
    MONO_REG(rcx, RCX);
    MONO_REG(rsp, RSP);
    MONO_REG(rip, RIP);
    MONO_REG(rflags, EFL);
#undef MONO_REG
    mc.mc_len = sizeof(Mcontext);
    mc.mc_fpformat = 0x10002; // FreeBSD _MC_FPFMT_XMM
    mc.mc_ownedfp = 0x20001;  // _MC_FPOWNED_FPU
    if (native->uc_mcontext.fpregs)
        std::memcpy(mc.mc_fpstate, native->uc_mcontext.fpregs, 512);
    return 0;
}
static int PS4_SYSV_ABI mono_resume_user_context(PthreadT thread) {
    if (!thread)
        return POSIX_ESRCH;
    int expected = 2;
    if (!thread->mono_suspend_state.compare_exchange_strong(expected, 3))
        return POSIX_EINVAL;
    MonoWake(thread->mono_suspend_state);
    while (thread->mono_suspend_state.load(std::memory_order_acquire) != 0)
        MonoWait(thread->mono_suspend_state, 3);
    return 0;
}
static void RegisterMonoSuspend(Core::Loader::SymbolsResolver* sym) {
    struct sigaction action{};
    action.sa_sigaction = MonoSuspendHandler;
    action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
    sigemptyset(&action.sa_mask);
    ASSERT(::sigaction(SIGRTMAX - 1, &action, nullptr) == 0);
    LIB_FUNCTION("cfjAjVTFG6A", "libkernel", 1, "libkernel", mono_suspend_user_context);
    LIB_FUNCTION("YkGOXpJEtO8", "libkernel", 1, "libkernel", mono_get_user_context);
    LIB_FUNCTION("QRdE7dBfNks", "libkernel", 1, "libkernel", mono_resume_user_context);
}
} // namespace Libraries::Kernel
#endif

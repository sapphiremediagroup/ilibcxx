#include <cstdint.hpp>
#include <syscall.hpp>

extern "C" __attribute__((naked)) std::uint64_t _syscall_impl(
    std::uint64_t number,
    std::uint64_t arg1,
    std::uint64_t arg2,
    std::uint64_t arg3,
    std::uint64_t arg4,
    std::uint64_t arg5
) {
    asm volatile (
        ".intel_syntax noprefix\n"
        "push rbx\n"
        "mov rax, rdi\n"
        "mov rbx, rsi\n"
        "mov r10, rdx\n"
        "mov rdx, rcx\n"
        "syscall\n"
        "pop rbx\n"
        "ret\n"
        ".att_syntax prefix\n"
    );
}

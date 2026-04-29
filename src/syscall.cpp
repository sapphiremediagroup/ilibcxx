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
        "mov rax, rcx\n"
        "mov rbx, rdx\n"
        "mov r10, r8\n"
        "mov rdx, r9\n"
        "mov r8, [rsp + 48]\n"
        "mov r9, [rsp + 56]\n"
        "syscall\n"
        "pop rbx\n"
        "ret\n"
        ".att_syntax prefix\n"
    );
}

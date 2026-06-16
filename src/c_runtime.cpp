#include <cstdlib.hpp>

extern "C" int main(int argc, char** argv);

extern "C" [[noreturn]] void userland_c_entry(unsigned long long argc, char** argv, char** envp) noexcept {
    (void)envp;
    std::exit(static_cast<std::uint64_t>(main(static_cast<int>(argc), argv)));
}

extern "C" __attribute__((naked, noreturn)) void _start() {
    asm volatile(
        ".intel_syntax noprefix\n"
        "mov rdi, [rsp]\n"
        "lea rsi, [rsp + 8]\n"
        "lea rdx, [rsi + rdi * 8 + 8]\n"
        "and rsp, -16\n"
        "call userland_c_entry\n"
        "1:\n"
        "jmp 1b\n"
        ".att_syntax prefix\n"
    );
}

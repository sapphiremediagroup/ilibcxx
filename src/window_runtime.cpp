#include <cstdlib.hpp>
#include <instant/window.hpp>

namespace instant {

int run_window_app(int argc, char** argv) {
    Window* window = instant_create_window(argc, argv);
    if (!window) {
        static constexpr char message[] = "[instant.window] instant_create_window returned null\n";
        std::serial_write(message, sizeof(message) - 1);
        return 1;
    }
    return window->run(argc, argv);
}

}

extern "C" [[noreturn]] void userland_window_entry(unsigned long long argc, char** argv, char** envp) noexcept {
    (void)envp;
    std::exit(static_cast<std::uint64_t>(instant::run_window_app(static_cast<int>(argc), argv)));
}

extern "C" __attribute__((naked, noreturn)) void _start() {
    asm volatile(
        ".intel_syntax noprefix\n"
        "mov rdi, [rsp]\n"
        "lea rsi, [rsp + 8]\n"
        "lea rdx, [rsi + rdi * 8 + 8]\n"
        "and rsp, -16\n"
        "call userland_window_entry\n"
        "1:\n"
        "jmp 1b\n"
        ".att_syntax prefix\n"
    );
}

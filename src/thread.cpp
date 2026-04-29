#include <cstdlib.hpp>
#include <syscall.hpp>

namespace {
struct ThreadStartContext {
    std::ThreadStartRoutine start;
    void* arg;
};
}

extern "C" [[noreturn]] void __instant_thread_entry(void* raw) noexcept {
    auto* context = static_cast<ThreadStartContext*>(raw);
    if (!context || !context->start) {
        std::thread_exit(static_cast<std::uint64_t>(-1));
    }

    std::ThreadStartRoutine start = context->start;
    void* arg = context->arg;
    std::free(context);

    start(arg);
    std::thread_exit(0);
}

namespace std {
ThreadHandle thread_create(ThreadStartRoutine start, void* arg, std::uint64_t stackSize) noexcept {
    if (!start) {
        return static_cast<ThreadHandle>(-1);
    }

    auto* context = static_cast<ThreadStartContext*>(std::malloc(sizeof(ThreadStartContext)));
    if (!context) {
        return static_cast<ThreadHandle>(-1);
    }

    context->start = start;
    context->arg = arg;

    ThreadHandle handle = _syscall_impl(
        static_cast<std::uint64_t>(Syscall::ThreadCreate),
        reinterpret_cast<std::uint64_t>(&__instant_thread_entry),
        reinterpret_cast<std::uint64_t>(context),
        stackSize
    );

    if (handle == static_cast<ThreadHandle>(-1)) {
        std::free(context);
    }

    return handle;
}

[[noreturn]] void thread_exit(std::uint64_t code) noexcept {
    _syscall_impl(static_cast<std::uint64_t>(Syscall::ThreadExit), code);
    for (;;) {
        asm volatile("");
    }
}

std::uint64_t thread_join(ThreadHandle handle, int* status) noexcept {
    return _syscall_impl(
        static_cast<std::uint64_t>(Syscall::ThreadJoin),
        handle,
        reinterpret_cast<std::uint64_t>(status)
    );
}
}

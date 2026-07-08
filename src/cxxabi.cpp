#include <syscall.hpp>
#include <cstring.hpp>

extern "C" {

namespace {
[[noreturn]] void panic(const char* message) {
    std::serial_write(message, std::strlen(message));
    std::exit(0);
}
}

#define MAX_ATEXIT 32

typedef void (*destructor_func)(void*);
typedef void (*init_func)(void);

struct atexit_entry {
    destructor_func func;
    void* arg;
    void* dso_handle;
};

static atexit_entry atexit_funcs[MAX_ATEXIT];
static int atexit_count = 0;

// .init_array/.fini_array boundary symbols. lld defines these as local hidden
// symbols in every module, but GNU ld does not provide them for a *shared*
// object whose .init_array is empty (libinstant has no dynamic constructors —
// there is no .init_array section, so _init/_fini iterate an empty range).
// Declaring them weak lets the reference resolve to 0 under GNU ld (start==end,
// empty loop) instead of becoming a GLOBAL-undefined dynamic symbol the runtime
// loader then fails on; harmless under lld, where they are still defined.
extern init_func __init_array_start[] __attribute__((weak));
extern init_func __init_array_end[] __attribute__((weak));
extern init_func __fini_array_start[] __attribute__((weak));
extern init_func __fini_array_end[] __attribute__((weak));

int __cxa_guard_acquire(long* guard) {
    return !*(char*)guard;
}

void __cxa_guard_release(long* guard) {
    *(char*)guard = 1;
}

void __cxa_guard_abort(long* guard) {
}

void __cxa_pure_virtual() {
    panic("Pure virtual function called!\n");
}

int __cxa_atexit(destructor_func func, void* arg, void* dso_handle) {
    if (atexit_count >= MAX_ATEXIT) {
        return -1;
    }
    
    atexit_funcs[atexit_count].func = func;
    atexit_funcs[atexit_count].arg = arg;
    atexit_funcs[atexit_count].dso_handle = dso_handle;
    atexit_count++;
    
    return 0;
}

void __cxa_finalize(void* dso_handle) {
    for (int i = atexit_count - 1; i >= 0; i--) {
        if (dso_handle == nullptr || atexit_funcs[i].dso_handle == dso_handle) {
            if (atexit_funcs[i].func) {
                atexit_funcs[i].func(atexit_funcs[i].arg);
            }
        }
    }
}

void _init() {
    for (init_func* func = __init_array_start; func < __init_array_end; func++) {
        (*func)();
    }
}

void _fini() {
    __cxa_finalize(nullptr);
    for (init_func* func = __fini_array_start; func < __fini_array_end; func++) {
        (*func)();
    }
}

}

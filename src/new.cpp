#include <syscall.hpp>
#include <cstring.hpp>
#include <new.hpp>

namespace {
    constexpr std::size_t HEAP_START = 0x0000600000000000ULL;
    constexpr std::size_t HEAP_SIZE = 16 * 1024 * 1024;
    std::size_t heap_current = HEAP_START;
    bool heap_initialized = false;

    [[noreturn]] void panic(const char* message) {
        std::serial_write(message, std::strlen(message));
        std::exit(0);
    }
    
    void init_heap() {
        if (!heap_initialized) {
            heap_initialized = true;
        }
    }
    
    void* allocate(std::size_t size) {
        init_heap();
        
        size = (size + 15) & ~static_cast<std::size_t>(15);
        
        if (heap_current + size >= HEAP_START + HEAP_SIZE) {
            return nullptr;
        }
        
        void* ptr = reinterpret_cast<void*>(heap_current);
        heap_current += size;
        return ptr;
    }
}

void* operator new(std::size_t size) {
    void* ptr = allocate(size);
    if (!ptr) {
        panic("Out of memory!\n");
    }
    return ptr;
}

void* operator new[](std::size_t size) {
    return operator new(size);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    return allocate(size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return allocate(size);
}

void operator delete(void*) noexcept {
}

void operator delete[](void*) noexcept {
}

void operator delete(void*, std::size_t) noexcept {
}

void operator delete[](void*, std::size_t) noexcept {
}

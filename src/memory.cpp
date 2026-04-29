#include <cstddef.hpp>
#include <cstdint.hpp>
#include <cstring.hpp>
#include <memory.hpp>
#include <new.hpp>
#include <syscall.hpp>

namespace {
constexpr std::size_t page_size = 4096;
constexpr std::size_t default_alignment = 16;

struct allocation_header {
    std::uint64_t mapping_size;
    std::uint64_t original_base;
};

[[nodiscard]] constexpr std::size_t align_up(std::size_t value, std::size_t alignment) noexcept {
    return (value + alignment - 1) & ~(alignment - 1);
}

[[nodiscard]] allocation_header* header_from_pointer(void* pointer) noexcept {
    auto* bytes = static_cast<unsigned char*>(pointer);
    return reinterpret_cast<allocation_header*>(bytes - sizeof(allocation_header));
}
}

namespace std {
void* malloc(size_t size) noexcept {
    if (size == 0) {
        size = 1;
    }

    const size_t payload_offset = align_up(sizeof(allocation_header), default_alignment);
    const size_t total_size = payload_offset + size;
    const size_t mapping_size = align_up(total_size, page_size);

    void* mapping = std::mmap(nullptr, mapping_size, 0);
    const auto mapping_address = reinterpret_cast<std::uint64_t>(mapping);
    if (mapping_address == static_cast<std::uint64_t>(-1) || mapping == nullptr) {
        return nullptr;
    }

    auto* payload = static_cast<unsigned char*>(mapping) + payload_offset;
    auto* header = header_from_pointer(payload);
    header->mapping_size = mapping_size;
    header->original_base = mapping_address;
    return payload;
}

void free(void* pointer) noexcept {
    if (pointer == nullptr) {
        return;
    }

    allocation_header* header = header_from_pointer(pointer);
    std::munmap(reinterpret_cast<void*>(header->original_base), header->mapping_size);
}

void* calloc(size_t count, size_t size) noexcept {
    if (count != 0 && size > static_cast<size_t>(-1) / count) {
        return nullptr;
    }

    void* pointer = std::malloc(count * size);
    if (pointer != nullptr) {
        std::memset(pointer, 0, count * size);
    }
    return pointer;
}

void* realloc(void* pointer, size_t size) noexcept {
    if (pointer == nullptr) {
        return std::malloc(size);
    }

    if (size == 0) {
        std::free(pointer);
        return nullptr;
    }

    allocation_header* old_header = header_from_pointer(pointer);
    const size_t payload_offset = align_up(sizeof(allocation_header), default_alignment);
    const size_t old_size = static_cast<size_t>(old_header->mapping_size) - payload_offset;

    void* new_pointer = std::malloc(size);
    if (new_pointer == nullptr) {
        return nullptr;
    }

    const size_t copy_size = old_size < size ? old_size : size;
    std::memcpy(new_pointer, pointer, copy_size);
    std::free(pointer);
    return new_pointer;
}
}

void* operator new(std::size_t size) {
    return std::malloc(size);
}

void* operator new[](std::size_t size) {
    return std::malloc(size);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    return std::malloc(size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return std::malloc(size);
}

void operator delete(void* pointer) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer) noexcept {
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer, std::size_t) noexcept {
    std::free(pointer);
}

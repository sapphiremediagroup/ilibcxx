#pragma once

#include <cstddef.hpp>

namespace std {
struct nothrow_t {
    explicit nothrow_t() = default;
};

inline constexpr nothrow_t nothrow{};
}

[[nodiscard]] void* operator new(std::size_t size);
[[nodiscard]] void* operator new[](std::size_t size);
[[nodiscard]] void* operator new(std::size_t size, const std::nothrow_t&) noexcept;
[[nodiscard]] void* operator new[](std::size_t size, const std::nothrow_t&) noexcept;

void operator delete(void* pointer) noexcept;
void operator delete[](void* pointer) noexcept;
void operator delete(void* pointer, std::size_t size) noexcept;
void operator delete[](void* pointer, std::size_t size) noexcept;

inline void* operator new(std::size_t, void* pointer) noexcept {
    return pointer;
}

inline void* operator new[](std::size_t, void* pointer) noexcept {
    return pointer;
}

inline void operator delete(void*, void*) noexcept {}
inline void operator delete[](void*, void*) noexcept {}

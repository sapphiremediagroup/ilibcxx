#pragma once

#include <cstddef.hpp>

extern "C" void __assert_fail(const char* expr, const char* file, int line, const char* func);

namespace std {
template <typename T>
struct remove_reference {
    using type = T;
};

template <typename T>
struct remove_reference<T&> {
    using type = T;
};

template <typename T>
struct remove_reference<T&&> {
    using type = T;
};

template <typename T>
using remove_reference_t = typename remove_reference<T>::type;

template <typename T>
[[nodiscard]] constexpr remove_reference_t<T>&& move(T&& value) noexcept {
    return static_cast<remove_reference_t<T>&&>(value);
}

template <typename T>
[[nodiscard]] constexpr T&& forward(remove_reference_t<T>& value) noexcept {
    return static_cast<T&&>(value);
}

template <typename T>
[[nodiscard]] constexpr T&& forward(remove_reference_t<T>&& value) noexcept {
    return static_cast<T&&>(value);
}

template <typename T>
void swap(T& left, T& right) noexcept {
    T tmp = std::move(left);
    left = std::move(right);
    right = std::move(tmp);
}

[[noreturn]] inline void libc_assert_fail(const char* expr, const char* file, int line, const char* func) {
    __assert_fail(expr, file, line, func);
    for (;;) {
        asm volatile("");
    }
}
}

#define STD_LIBC_ASSERT(expr) ((expr) ? (void)0 : ::std::libc_assert_fail(#expr, __FILE__, __LINE__, __func__))

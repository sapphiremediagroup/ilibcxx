#pragma once

#include <cstddef.hpp>

namespace std {
template <typename T>
class initializer_list {
public:
    using value_type = T;
    using reference = const T&;
    using const_reference = const T&;
    using size_type = size_t;
    using iterator = const T*;
    using const_iterator = const T*;

    constexpr initializer_list() noexcept : data_(nullptr), size_(0) {}

    [[nodiscard]] constexpr size_t size() const noexcept { return size_; }
    [[nodiscard]] constexpr const T* begin() const noexcept { return data_; }
    [[nodiscard]] constexpr const T* end() const noexcept { return data_ + size_; }

private:
    constexpr initializer_list(const T* data, size_t size) noexcept : data_(data), size_(size) {}

    const T* data_;
    size_t size_;
};

template <typename T>
[[nodiscard]] constexpr const T* begin(initializer_list<T> list) noexcept {
    return list.begin();
}

template <typename T>
[[nodiscard]] constexpr const T* end(initializer_list<T> list) noexcept {
    return list.end();
}
}

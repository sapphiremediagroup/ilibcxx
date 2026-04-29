#pragma once

#include <cstddef.hpp>
#include <utility.hpp>

namespace std {
template <typename T>
class unique_ptr {
public:
    unique_ptr() noexcept : pointer_(nullptr) {}
    explicit unique_ptr(T* pointer) noexcept : pointer_(pointer) {}

    unique_ptr(const unique_ptr&) = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;

    unique_ptr(unique_ptr&& other) noexcept : pointer_(other.release()) {}

    unique_ptr& operator=(unique_ptr&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    ~unique_ptr() {
        reset();
    }

    [[nodiscard]] T* get() const noexcept { return pointer_; }
    [[nodiscard]] explicit operator bool() const noexcept { return pointer_ != nullptr; }

    [[nodiscard]] T* release() noexcept {
        T* out = pointer_;
        pointer_ = nullptr;
        return out;
    }

    void reset(T* pointer = nullptr) noexcept {
        T* old = pointer_;
        pointer_ = pointer;
        delete old;
    }

    [[nodiscard]] T& operator*() const {
        STD_LIBC_ASSERT(pointer_ != nullptr);
        return *pointer_;
    }

    [[nodiscard]] T* operator->() const noexcept {
        return pointer_;
    }

private:
    T* pointer_;
};

template <typename T, typename... Args>
[[nodiscard]] unique_ptr<T> make_unique(Args&&... args) {
    return unique_ptr<T>(new T(std::forward<Args>(args)...));
}
}

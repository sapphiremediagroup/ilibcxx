#pragma once

#include <cstddef.hpp>
#include <memory.hpp>
#include <utility.hpp>

namespace std {
template <typename T>
class shared_ptr {
public:
    shared_ptr() noexcept : pointer_(nullptr), count_(nullptr) {}

    explicit shared_ptr(T* pointer) : pointer_(pointer), count_(nullptr) {
        if (pointer_) {
            count_ = static_cast<size_t*>(std::malloc(sizeof(size_t)));
            STD_LIBC_ASSERT(count_ != nullptr);
            *count_ = 1;
        }
    }

    shared_ptr(const shared_ptr& other) noexcept : pointer_(other.pointer_), count_(other.count_) {
        if (count_) {
            ++*count_;
        }
    }

    shared_ptr(shared_ptr&& other) noexcept : pointer_(other.pointer_), count_(other.count_) {
        other.pointer_ = nullptr;
        other.count_ = nullptr;
    }

    ~shared_ptr() {
        reset();
    }

    shared_ptr& operator=(const shared_ptr& other) noexcept {
        if (this != &other) {
            reset();
            pointer_ = other.pointer_;
            count_ = other.count_;
            if (count_) {
                ++*count_;
            }
        }
        return *this;
    }

    shared_ptr& operator=(shared_ptr&& other) noexcept {
        if (this != &other) {
            reset();
            pointer_ = other.pointer_;
            count_ = other.count_;
            other.pointer_ = nullptr;
            other.count_ = nullptr;
        }
        return *this;
    }

    [[nodiscard]] T* get() const noexcept { return pointer_; }
    [[nodiscard]] size_t use_count() const noexcept { return count_ ? *count_ : 0; }
    [[nodiscard]] explicit operator bool() const noexcept { return pointer_ != nullptr; }

    void reset(T* pointer = nullptr) {
        release_current();
        pointer_ = pointer;
        count_ = nullptr;
        if (pointer_) {
            count_ = static_cast<size_t*>(std::malloc(sizeof(size_t)));
            STD_LIBC_ASSERT(count_ != nullptr);
            *count_ = 1;
        }
    }

    [[nodiscard]] T& operator*() const {
        STD_LIBC_ASSERT(pointer_ != nullptr);
        return *pointer_;
    }

    [[nodiscard]] T* operator->() const noexcept {
        return pointer_;
    }

private:
    void release_current() noexcept {
        if (!count_) {
            return;
        }
        --*count_;
        if (*count_ == 0) {
            delete pointer_;
            std::free(count_);
        }
        pointer_ = nullptr;
        count_ = nullptr;
    }

    T* pointer_;
    size_t* count_;
};

template <typename T, typename... Args>
[[nodiscard]] shared_ptr<T> make_shared(Args&&... args) {
    return shared_ptr<T>(new T(std::forward<Args>(args)...));
}
}

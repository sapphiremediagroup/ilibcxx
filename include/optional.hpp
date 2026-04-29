#pragma once

#include <cstddef.hpp>
#include <new.hpp>
#include <utility.hpp>

namespace std {
struct nullopt_t {
    explicit constexpr nullopt_t(int) {}
};

inline constexpr nullopt_t nullopt{0};

template <typename T>
class optional {
public:
    optional() noexcept : engaged_(false) {}
    optional(nullopt_t) noexcept : engaged_(false) {}

    optional(const T& value) : engaged_(false) {
        emplace(value);
    }

    optional(T&& value) : engaged_(false) {
        emplace(std::move(value));
    }

    optional(const optional& other) : engaged_(false) {
        if (other.engaged_) {
            emplace(*other.ptr());
        }
    }

    optional(optional&& other) : engaged_(false) {
        if (other.engaged_) {
            emplace(std::move(*other.ptr()));
        }
    }

    ~optional() {
        reset();
    }

    optional& operator=(nullopt_t) noexcept {
        reset();
        return *this;
    }

    optional& operator=(const optional& other) {
        if (this != &other) {
            if (other.engaged_) {
                if (engaged_) {
                    value() = *other;
                } else {
                    emplace(*other);
                }
            } else {
                reset();
            }
        }
        return *this;
    }

    optional& operator=(optional&& other) {
        if (this != &other) {
            if (other.engaged_) {
                if (engaged_) {
                    value() = std::move(*other);
                } else {
                    emplace(std::move(*other));
                }
            } else {
                reset();
            }
        }
        return *this;
    }

    [[nodiscard]] bool has_value() const noexcept { return engaged_; }
    [[nodiscard]] explicit operator bool() const noexcept { return engaged_; }

    [[nodiscard]] T& value() {
        STD_LIBC_ASSERT(engaged_);
        return *ptr();
    }

    [[nodiscard]] const T& value() const {
        STD_LIBC_ASSERT(engaged_);
        return *ptr();
    }

    template <typename U>
    [[nodiscard]] T value_or(U&& fallback) const {
        return engaged_ ? *ptr() : static_cast<T>(std::forward<U>(fallback));
    }

    void reset() noexcept {
        if (engaged_) {
            ptr()->~T();
            engaged_ = false;
        }
    }

    template <typename... Args>
    T& emplace(Args&&... args) {
        reset();
        new (storage_) T(std::forward<Args>(args)...);
        engaged_ = true;
        return *ptr();
    }

    [[nodiscard]] T& operator*() { return value(); }
    [[nodiscard]] const T& operator*() const { return value(); }
    [[nodiscard]] T* operator->() { return &value(); }
    [[nodiscard]] const T* operator->() const { return &value(); }

private:
    [[nodiscard]] T* ptr() noexcept {
        return reinterpret_cast<T*>(storage_);
    }

    [[nodiscard]] const T* ptr() const noexcept {
        return reinterpret_cast<const T*>(storage_);
    }

    alignas(T) unsigned char storage_[sizeof(T)];
    bool engaged_;
};
}

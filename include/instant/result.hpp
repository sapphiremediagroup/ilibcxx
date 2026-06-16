#pragma once

#include <new.hpp>
#include <utility.hpp>

namespace instant {

template <typename T, typename E>
class Result {
public:
    Result(const T& value) : ok_(true) {
        new (value_storage_) T(value);
    }

    Result(T&& value) : ok_(true) {
        new (value_storage_) T(std::move(value));
    }

    Result(const Result& other) : ok_(other.ok_) {
        if (ok_) {
            new (value_storage_) T(other.value());
        } else {
            new (error_storage_) E(other.error());
        }
    }

    Result(Result&& other) noexcept : ok_(other.ok_) {
        if (ok_) {
            new (value_storage_) T(std::move(other.value()));
        } else {
            new (error_storage_) E(std::move(other.error()));
        }
    }

    ~Result() {
        reset();
    }

    Result& operator=(const Result& other) {
        if (this != &other) {
            this->~Result();
            new (this) Result(other);
        }
        return *this;
    }

    Result& operator=(Result&& other) noexcept {
        if (this != &other) {
            this->~Result();
            new (this) Result(std::move(other));
        }
        return *this;
    }

    static Result ok(const T& value) {
        return Result(value);
    }

    static Result ok(T&& value) {
        return Result(std::move(value));
    }

    static Result error(const E& error) {
        return Result(ErrorTag {}, error);
    }

    static Result error(E&& error) {
        return Result(ErrorTag {}, std::move(error));
    }

    static Result err(const E& error) {
        return Result(ErrorTag {}, error);
    }

    static Result err(E&& error) {
        return Result(ErrorTag {}, std::move(error));
    }

    [[nodiscard]] bool is_ok() const noexcept { return ok_; }
    [[nodiscard]] bool is_error() const noexcept { return !ok_; }
    [[nodiscard]] explicit operator bool() const noexcept { return ok_; }

    [[nodiscard]] T& value() noexcept { return *value_ptr(); }
    [[nodiscard]] const T& value() const noexcept { return *value_ptr(); }
    [[nodiscard]] E& error() noexcept { return *error_ptr(); }
    [[nodiscard]] const E& error() const noexcept { return *error_ptr(); }

    template <typename U>
    [[nodiscard]] T value_or(U&& fallback) const {
        return ok_ ? value() : static_cast<T>(std::forward<U>(fallback));
    }

private:
    struct ErrorTag {};

    Result(ErrorTag, const E& error) : ok_(false) {
        new (error_storage_) E(error);
    }

    Result(ErrorTag, E&& error) : ok_(false) {
        new (error_storage_) E(std::move(error));
    }

    void reset() noexcept {
        if (ok_) {
            value_ptr()->~T();
        } else {
            error_ptr()->~E();
        }
    }

    [[nodiscard]] T* value_ptr() noexcept {
        return reinterpret_cast<T*>(value_storage_);
    }

    [[nodiscard]] const T* value_ptr() const noexcept {
        return reinterpret_cast<const T*>(value_storage_);
    }

    [[nodiscard]] E* error_ptr() noexcept {
        return reinterpret_cast<E*>(error_storage_);
    }

    [[nodiscard]] const E* error_ptr() const noexcept {
        return reinterpret_cast<const E*>(error_storage_);
    }

    bool ok_;
    alignas(T) unsigned char value_storage_[sizeof(T)];
    alignas(E) unsigned char error_storage_[sizeof(E)];
};

}

template <typename T, typename E>
using Result = ::instant::Result<T, E>;

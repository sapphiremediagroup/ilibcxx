#pragma once

#include <cstddef.hpp>
#include <memory.hpp>
#include <new.hpp>
#include <utility.hpp>

namespace std {
template <typename T>
class vector {
public:
    using value_type = T;
    using iterator = T*;
    using const_iterator = const T*;

    vector() noexcept : data_(nullptr), size_(0), capacity_(0) {}

    vector(const vector& other) : vector() {
        reserve(other.size_);
        for (size_t i = 0; i < other.size_; ++i) {
            emplace_back(other.data_[i]);
        }
    }

    vector(vector&& other) noexcept
        : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    ~vector() {
        clear();
        std::free(data_);
    }

    vector& operator=(const vector& other) {
        if (this != &other) {
            clear();
            reserve(other.size_);
            for (size_t i = 0; i < other.size_; ++i) {
                emplace_back(other.data_[i]);
            }
        }
        return *this;
    }

    vector& operator=(vector&& other) noexcept {
        if (this != &other) {
            clear();
            std::free(data_);
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            other.data_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
        }
        return *this;
    }

    void push_back(const T& value) {
        emplace_back(value);
    }

    void push_back(T&& value) {
        emplace_back(std::move(value));
    }

    template <typename... Args>
    T& emplace_back(Args&&... args) {
        if (size_ == capacity_) {
            reserve(capacity_ == 0 ? 4 : capacity_ * 2);
        }
        new (data_ + size_) T(std::forward<Args>(args)...);
        ++size_;
        return back();
    }

    void pop_back() {
        STD_LIBC_ASSERT(size_ != 0);
        --size_;
        data_[size_].~T();
    }

    [[nodiscard]] T& operator[](size_t index) noexcept { return data_[index]; }
    [[nodiscard]] const T& operator[](size_t index) const noexcept { return data_[index]; }

    [[nodiscard]] T& at(size_t index) {
        STD_LIBC_ASSERT(index < size_);
        return data_[index];
    }

    [[nodiscard]] const T& at(size_t index) const {
        STD_LIBC_ASSERT(index < size_);
        return data_[index];
    }

    [[nodiscard]] T& front() {
        STD_LIBC_ASSERT(size_ != 0);
        return data_[0];
    }

    [[nodiscard]] const T& front() const {
        STD_LIBC_ASSERT(size_ != 0);
        return data_[0];
    }

    [[nodiscard]] T& back() {
        STD_LIBC_ASSERT(size_ != 0);
        return data_[size_ - 1];
    }

    [[nodiscard]] const T& back() const {
        STD_LIBC_ASSERT(size_ != 0);
        return data_[size_ - 1];
    }

    [[nodiscard]] size_t size() const noexcept { return size_; }
    [[nodiscard]] size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    void reserve(size_t requested) {
        if (requested <= capacity_) {
            return;
        }
        T* new_data = static_cast<T*>(std::malloc(sizeof(T) * requested));
        STD_LIBC_ASSERT(new_data != nullptr);
        for (size_t i = 0; i < size_; ++i) {
            new (new_data + i) T(std::move(data_[i]));
            data_[i].~T();
        }
        std::free(data_);
        data_ = new_data;
        capacity_ = requested;
    }

    void resize(size_t requested) {
        if (requested < size_) {
            while (size_ > requested) {
                pop_back();
            }
            return;
        }
        reserve(requested);
        while (size_ < requested) {
            emplace_back();
        }
    }

    void clear() noexcept {
        for (size_t i = 0; i < size_; ++i) {
            data_[i].~T();
        }
        size_ = 0;
    }

    [[nodiscard]] iterator begin() noexcept { return data_; }
    [[nodiscard]] iterator end() noexcept { return data_ + size_; }
    [[nodiscard]] const_iterator begin() const noexcept { return data_; }
    [[nodiscard]] const_iterator end() const noexcept { return data_ + size_; }

private:
    T* data_;
    size_t size_;
    size_t capacity_;
};
}

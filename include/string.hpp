#pragma once

#include <cstddef.hpp>
#include <cstring.hpp>
#include <memory.hpp>
#include <utility.hpp>

namespace std {
class string {
public:
    static constexpr size_t npos = static_cast<size_t>(-1);

    string() noexcept : data_(small_), size_(0), capacity_(small_capacity) {
        small_[0] = '\0';
    }

    string(const char* text) : string() {
        append(text ? text : "");
    }

    string(const char* text, size_t count) : string() {
        append(text ? text : "", count);
    }

    string(const string& other) : string() {
        append(other.data_, other.size_);
    }

    string(string&& other) noexcept : data_(small_), size_(0), capacity_(small_capacity) {
        if (other.is_small()) {
            std::memcpy(small_, other.small_, other.size_ + 1);
            size_ = other.size_;
        } else {
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            other.data_ = other.small_;
            other.size_ = 0;
            other.capacity_ = small_capacity;
            other.small_[0] = '\0';
        }
    }

    ~string() {
        if (!is_small()) {
            std::free(data_);
        }
    }

    string& operator=(const string& other) {
        if (this != &other) {
            clear();
            append(other.data_, other.size_);
        }
        return *this;
    }

    string& operator=(string&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        if (!is_small()) {
            std::free(data_);
        }
        data_ = small_;
        size_ = 0;
        capacity_ = small_capacity;
        small_[0] = '\0';

        if (other.is_small()) {
            std::memcpy(small_, other.small_, other.size_ + 1);
            size_ = other.size_;
        } else {
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            other.data_ = other.small_;
            other.size_ = 0;
            other.capacity_ = small_capacity;
            other.small_[0] = '\0';
        }
        return *this;
    }

    string& operator=(const char* text) {
        clear();
        append(text ? text : "");
        return *this;
    }

    [[nodiscard]] const char* c_str() const noexcept { return data_; }
    [[nodiscard]] char* data() noexcept { return data_; }
    [[nodiscard]] const char* data() const noexcept { return data_; }
    [[nodiscard]] size_t size() const noexcept { return size_; }
    [[nodiscard]] size_t length() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    void clear() noexcept {
        size_ = 0;
        data_[0] = '\0';
    }

    string& append(const char* text) {
        return append(text, text ? std::strlen(text) : 0);
    }

    string& append(const char* text, size_t count) {
        if (count == 0) {
            return *this;
        }
        STD_LIBC_ASSERT(text != nullptr);
        reserve(size_ + count);
        std::memcpy(data_ + size_, text, count);
        size_ += count;
        data_[size_] = '\0';
        return *this;
    }

    string& append(const string& other) {
        return append(other.data_, other.size_);
    }

    string& operator+=(const string& other) {
        return append(other);
    }

    string& operator+=(const char* text) {
        return append(text);
    }

    string& operator+=(char ch) {
        return append(&ch, 1);
    }

    [[nodiscard]] string substr(size_t pos, size_t count = npos) const {
        STD_LIBC_ASSERT(pos <= size_);
        size_t available = size_ - pos;
        if (count == npos || count > available) {
            count = available;
        }
        return string(data_ + pos, count);
    }

    [[nodiscard]] size_t find(const string& needle, size_t pos = 0) const noexcept {
        return find(needle.data_, pos);
    }

    [[nodiscard]] size_t find(const char* needle, size_t pos = 0) const noexcept {
        if (!needle || pos > size_) {
            return npos;
        }
        size_t needle_size = std::strlen(needle);
        if (needle_size == 0) {
            return pos;
        }
        if (needle_size > size_) {
            return npos;
        }
        for (size_t i = pos; i + needle_size <= size_; ++i) {
            if (std::memcmp(data_ + i, needle, needle_size) == 0) {
                return i;
            }
        }
        return npos;
    }

    void reserve(size_t requested) {
        if (requested <= capacity_) {
            return;
        }
        size_t next = capacity_ * 2;
        if (next < requested) {
            next = requested;
        }
        char* new_data = static_cast<char*>(std::malloc(next + 1));
        STD_LIBC_ASSERT(new_data != nullptr);
        std::memcpy(new_data, data_, size_ + 1);
        if (!is_small()) {
            std::free(data_);
        }
        data_ = new_data;
        capacity_ = next;
    }

    [[nodiscard]] char& operator[](size_t index) noexcept { return data_[index]; }
    [[nodiscard]] const char& operator[](size_t index) const noexcept { return data_[index]; }

    [[nodiscard]] char* begin() noexcept { return data_; }
    [[nodiscard]] char* end() noexcept { return data_ + size_; }
    [[nodiscard]] const char* begin() const noexcept { return data_; }
    [[nodiscard]] const char* end() const noexcept { return data_ + size_; }

private:
    static constexpr size_t small_capacity = 23;

    [[nodiscard]] bool is_small() const noexcept {
        return data_ == small_;
    }

    char* data_;
    size_t size_;
    size_t capacity_;
    char small_[small_capacity + 1];
};

inline bool operator==(const string& left, const string& right) noexcept {
    return left.size() == right.size() && std::memcmp(left.data(), right.data(), left.size()) == 0;
}

inline bool operator!=(const string& left, const string& right) noexcept {
    return !(left == right);
}

inline bool operator<(const string& left, const string& right) noexcept {
    size_t count = left.size() < right.size() ? left.size() : right.size();
    int cmp = std::memcmp(left.data(), right.data(), count);
    if (cmp != 0) {
        return cmp < 0;
    }
    return left.size() < right.size();
}

inline string operator+(const string& left, const string& right) {
    string result(left);
    result += right;
    return result;
}

inline string operator+(const string& left, const char* right) {
    string result(left);
    result += right;
    return result;
}

inline string operator+(const char* left, const string& right) {
    string result(left);
    result += right;
    return result;
}
}

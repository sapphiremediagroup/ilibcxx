#pragma once

#include <cstddef.hpp>

namespace std {
inline size_t strlen(const char* string) noexcept {
    size_t length = 0;
    while (string[length] != '\0') {
        ++length;
    }
    return length;
}

inline int strcmp(const char* left, const char* right) noexcept {
    while (*left != '\0' && *left == *right) {
        ++left;
        ++right;
    }
    return static_cast<unsigned char>(*left) - static_cast<unsigned char>(*right);
}

inline int strncmp(const char* left, const char* right, size_t count) noexcept {
    while (count != 0 && *left != '\0' && *left == *right) {
        ++left;
        ++right;
        --count;
    }
    if (count == 0) {
        return 0;
    }
    return static_cast<unsigned char>(*left) - static_cast<unsigned char>(*right);
}

inline char* strcpy(char* destination, const char* source) noexcept {
    char* out = destination;
    while (*source != '\0') {
        *out++ = *source++;
    }
    *out = '\0';
    return destination;
}

inline char* strncpy(char* destination, const char* source, size_t count) noexcept {
    char* out = destination;
    size_t index = 0;
    while (index < count && *source != '\0') {
        *out++ = *source++;
        ++index;
    }
    while (index < count) {
        *out++ = '\0';
        ++index;
    }
    return destination;
}

inline int memcmp(const void* left, const void* right, size_t count) noexcept {
    const auto* lhs = static_cast<const unsigned char*>(left);
    const auto* rhs = static_cast<const unsigned char*>(right);
    for (size_t index = 0; index < count; ++index) {
        if (lhs[index] != rhs[index]) {
            return static_cast<int>(lhs[index]) - static_cast<int>(rhs[index]);
        }
    }
    return 0;
}

inline void* memcpy(void* destination, const void* source, size_t count) noexcept {
    auto* out = static_cast<unsigned char*>(destination);
    const auto* in = static_cast<const unsigned char*>(source);
    for (size_t index = 0; index < count; ++index) {
        out[index] = in[index];
    }
    return destination;
}

inline void* memmove(void* destination, const void* source, size_t count) noexcept {
    auto* out = static_cast<unsigned char*>(destination);
    const auto* in = static_cast<const unsigned char*>(source);
    if (out < in) {
        for (size_t index = 0; index < count; ++index) {
            out[index] = in[index];
        }
    } else if (out > in) {
        for (size_t index = count; index > 0; --index) {
            out[index - 1] = in[index - 1];
        }
    }
    return destination;
}

inline void* memset(void* destination, int value, size_t count) noexcept {
    auto* out = static_cast<unsigned char*>(destination);
    for (size_t index = 0; index < count; ++index) {
        out[index] = static_cast<unsigned char>(value);
    }
    return destination;
}
}

#pragma once

#include <cstddef.hpp>
#include <cstdint.hpp>
#include <cstdarg.hpp>
#include <cstring.hpp>

namespace std {
inline int vsnprintf(char* buffer, size_t size, const char* format, std::va_list args) noexcept {
    if (format == nullptr) {
        return -1;
    }

    size_t written = 0;
    char temp[32];

    auto append_char = [&](char c) {
        if (written + 1 < size) {
            buffer[written] = c;
        }
        ++written;
    };

    auto append_buffer = [&](const char* data, size_t length) {
        for (size_t i = 0; i < length; ++i) {
            append_char(data[i]);
        }
    };

    const char* current = format;
    while (*current != '\0') {
        if (*current != '%') {
            append_char(*current);
            ++current;
            continue;
        }

        ++current;
        if (*current == '%') {
            append_char('%');
            ++current;
            continue;
        }

        bool long_long = false;
        if (*current == 'l') {
            ++current;
            if (*current == 'l') {
                long_long = true;
                ++current;
            }
        }

        char specifier = *current;
        if (specifier == '\0') {
            break;
        }

        if (specifier == 's') {
            const char* string = va_arg(args, const char*);
            if (string == nullptr) {
                string = "(null)";
            }
            append_buffer(string, std::strlen(string));
        } else if (specifier == 'c') {
            char value = static_cast<char>(va_arg(args, int));
            append_char(value);
        } else if (specifier == 'd' || specifier == 'i') {
            long long value = long_long ? va_arg(args, long long) : va_arg(args, int);
            bool negative = value < 0;
            unsigned long long unsigned_value = negative ? static_cast<unsigned long long>(-value) : static_cast<unsigned long long>(value);
            size_t index = 0;
            if (unsigned_value == 0) {
                temp[index++] = '0';
            } else {
                while (unsigned_value != 0 && index < sizeof(temp) - 1) {
                    temp[index++] = static_cast<char>('0' + (unsigned_value % 10));
                    unsigned_value /= 10;
                }
            }
            if (negative) {
                temp[index++] = '-';
            }
            for (size_t i = 0; i < index / 2; ++i) {
                char swap = temp[i];
                temp[i] = temp[index - i - 1];
                temp[index - i - 1] = swap;
            }
            append_buffer(temp, index);
        } else if (specifier == 'u') {
            unsigned long long value = long_long ? va_arg(args, unsigned long long) : va_arg(args, unsigned int);
            size_t index = 0;
            if (value == 0) {
                temp[index++] = '0';
            } else {
                while (value != 0 && index < sizeof(temp) - 1) {
                    temp[index++] = static_cast<char>('0' + (value % 10));
                    value /= 10;
                }
            }
            for (size_t i = 0; i < index / 2; ++i) {
                char swap = temp[i];
                temp[i] = temp[index - i - 1];
                temp[index - i - 1] = swap;
            }
            append_buffer(temp, index);
        } else if (specifier == 'x' || specifier == 'X' || specifier == 'p') {
            unsigned long long value = specifier == 'p' ? reinterpret_cast<unsigned long long>(va_arg(args, void*)) : (long_long ? va_arg(args, unsigned long long) : va_arg(args, unsigned int));
            const char* digits = specifier == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
            size_t index = 0;
            if (value == 0) {
                temp[index++] = '0';
            } else {
                while (value != 0 && index < sizeof(temp) - 1) {
                    temp[index++] = digits[value & 0xF];
                    value >>= 4;
                }
            }
            if (specifier == 'p') {
                temp[index++] = 'x';
                temp[index++] = '0';
            }
            for (size_t i = 0; i < index / 2; ++i) {
                char swap = temp[i];
                temp[i] = temp[index - i - 1];
                temp[index - i - 1] = swap;
            }
            append_buffer(temp, index);
        } else {
            append_char('%');
            append_char(specifier);
        }

        ++current;
    }

    if (size != 0) {
        const size_t write_count = written < (size - 1) ? written : (size - 1);
        if (buffer != nullptr) {
            buffer[write_count] = '\0';
        }
    }

    return static_cast<int>(written);
}

inline int snprintf(char* buffer, size_t size, const char* format, ...) noexcept {
    std::va_list args;
    va_start(args, format);
    int result = vsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}
}

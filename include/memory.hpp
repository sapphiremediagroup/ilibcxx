#pragma once

#include <cstddef.hpp>

namespace std {
[[nodiscard]] void* malloc(size_t size) noexcept;
[[nodiscard]] void* calloc(size_t count, size_t size) noexcept;
[[nodiscard]] void* realloc(void* pointer, size_t size) noexcept;
void free(void* pointer) noexcept;
}

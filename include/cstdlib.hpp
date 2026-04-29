#pragma once

#include <cstddef.hpp>
#include <cstdint.hpp>
#include <memory.hpp>
#include <syscall.hpp>

namespace std {
[[nodiscard]] inline int abs(int value) noexcept {
    return value < 0 ? -value : value;
}
}

#pragma once

namespace std {
using ptrdiff_t = decltype(static_cast<int*>(nullptr) - static_cast<int*>(nullptr));
using size_t = decltype(sizeof(0));
using nullptr_t = decltype(nullptr);
}

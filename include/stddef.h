#ifndef LIBC_STDDEF_H
#define LIBC_STDDEF_H

#ifdef __cplusplus
#include <cstddef.hpp>
#include <cstdint.hpp>
extern "C" {
typedef std::size_t size_t;
typedef std::ptrdiff_t ptrdiff_t;
typedef std::uintptr_t uintptr_t;
typedef std::intptr_t intptr_t;
}
#else
/* Prefer compiler provided underlying typedefs when available */
#if defined(__SIZE_TYPE__)
typedef __SIZE_TYPE__ size_t;
#else
typedef unsigned long size_t;
#endif

#if defined(__PTRDIFF_TYPE__)
typedef __PTRDIFF_TYPE__ ptrdiff_t;
#else
typedef long ptrdiff_t;
#endif

#if defined(__UINTPTR_TYPE__)
typedef __UINTPTR_TYPE__ uintptr_t;
#else
typedef unsigned long uintptr_t;
#endif

#if defined(__INTPTR_TYPE__)
typedef __INTPTR_TYPE__ intptr_t;
#else
typedef long intptr_t;
#endif

#endif /* __cplusplus */

#ifndef NULL
#define NULL ((void*)0)
#endif

#endif /* LIBC_STDDEF_H */

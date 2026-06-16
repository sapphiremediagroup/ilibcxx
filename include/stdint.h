#ifndef LIBC_STDINT_H
#define LIBC_STDINT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Prefer compiler-provided type macros when available */
#if defined(__INT8_TYPE__)
typedef __INT8_TYPE__ int8_t;
#else
typedef signed char int8_t;
#endif

#if defined(__UINT8_TYPE__)
typedef __UINT8_TYPE__ uint8_t;
#else
typedef unsigned char uint8_t;
#endif

#if defined(__INT16_TYPE__)
typedef __INT16_TYPE__ int16_t;
#else
typedef short int16_t;
#endif

#if defined(__UINT16_TYPE__)
typedef __UINT16_TYPE__ uint16_t;
#else
typedef unsigned short uint16_t;
#endif

#if defined(__INT32_TYPE__)
typedef __INT32_TYPE__ int32_t;
#else
typedef int int32_t;
#endif

#if defined(__UINT32_TYPE__)
typedef __UINT32_TYPE__ uint32_t;
#else
typedef unsigned int uint32_t;
#endif

#if defined(__INT64_TYPE__)
typedef __INT64_TYPE__ int64_t;
#else
typedef long long int64_t;
#endif

#if defined(__UINT64_TYPE__)
typedef __UINT64_TYPE__ uint64_t;
#else
typedef unsigned long long uint64_t;
#endif

#if defined(__INTPTR_TYPE__)
typedef __INTPTR_TYPE__ intptr_t;
#else
typedef long intptr_t;
#endif

#if defined(__UINTPTR_TYPE__)
typedef __UINTPTR_TYPE__ uintptr_t;
#else
typedef unsigned long uintptr_t;
#endif

#if defined(__INTMAX_TYPE__)
typedef __INTMAX_TYPE__ intmax_t;
#else
typedef long long intmax_t;
#endif

#if defined(__UINTMAX_TYPE__)
typedef __UINTMAX_TYPE__ uintmax_t;
#else
typedef unsigned long long uintmax_t;
#endif

#ifdef __cplusplus
}
#endif

#endif

#ifndef LIBC_WCHAR_H
#define LIBC_WCHAR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Provide wchar_t and wint_t when compiling as C or when compiler doesn't supply them */
#ifndef __cplusplus
#if defined(__WCHAR_TYPE__)
typedef __WCHAR_TYPE__ wchar_t;
#else
typedef unsigned int wchar_t;
#endif
#endif

#if defined(__WINT_TYPE__)
typedef __WINT_TYPE__ wint_t;
#else
typedef int wint_t;
#endif

#ifndef WEOF
#define WEOF ((wint_t)-1)
#endif

#ifdef __cplusplus
}
#endif

#endif

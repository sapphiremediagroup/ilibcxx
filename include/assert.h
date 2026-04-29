#ifndef LIBC_ASSERT_H
#define LIBC_ASSERT_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

void __assert_fail(const char* expr, const char* file, int line, const char* func);

#ifndef NDEBUG
#define assert(expr) ((expr) ? (void)0 : __assert_fail(#expr, __FILE__, __LINE__, __func__))
#else
#define assert(ignore) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif

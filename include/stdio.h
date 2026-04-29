#ifndef LIBC_STDIO_H
#define LIBC_STDIO_H

#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

int vsnprintf(char* buffer, size_t size, const char* format, va_list args);
int snprintf(char* buffer, size_t size, const char* format, ...);
int printf(const char* format, ...);
int putchar(int c);

#ifdef __cplusplus
}
#endif

#endif

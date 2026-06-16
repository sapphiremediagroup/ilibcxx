#ifndef LIBC_STDLIB_H
#define LIBC_STDLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#define LIBCXX_NOEXCEPT noexcept
#else
#define LIBCXX_NOEXCEPT
#endif

void* malloc(size_t size) LIBCXX_NOEXCEPT;
void free(void* ptr) LIBCXX_NOEXCEPT;
void* realloc(void* ptr, size_t size) LIBCXX_NOEXCEPT;
void* calloc(size_t nmemb, size_t size) LIBCXX_NOEXCEPT;
int abs(int x) LIBCXX_NOEXCEPT;
int atoi(const char* nptr);
long strtol(const char* nptr, char** endptr, int base);
unsigned long strtoul(const char* nptr, char** endptr, int base);
unsigned long long strtoull(const char* nptr, char** endptr, int base);
float strtof(const char* nptr, char** endptr);
double strtod(const char* nptr, char** endptr);
long double strtold(const char* nptr, char** endptr);
char* getenv(const char* name);
void qsort(void* base, size_t nmemb, size_t size,
           int (*compar)(const void*, const void*));

void exit(int status);
void abort(void);

#define RAND_MAX 2147483647
int rand(void) LIBCXX_NOEXCEPT;
void srand(unsigned int seed) LIBCXX_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#undef LIBCXX_NOEXCEPT

#endif

#ifndef LIBC_STDLIB_H
#define LIBC_STDLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* malloc(size_t size);
void free(void* ptr);
void* realloc(void* ptr, size_t size);
void* calloc(size_t nmemb, size_t size);
int abs(int x);
long strtol(const char* nptr, char** endptr, int base);
double strtod(const char* nptr, char** endptr);
char* getenv(const char* name);
void qsort(void* base, size_t nmemb, size_t size,
           int (*compar)(const void*, const void*));

void exit(int status);
void abort(void);

#define RAND_MAX 2147483647
int rand(void);
void srand(unsigned int seed);

#ifdef __cplusplus
}
#endif

#endif

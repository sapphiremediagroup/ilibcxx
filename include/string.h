#ifndef LIBC_STRING_H
#define LIBC_STRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t strlen(const char* s);
void* memchr(const void* s, int c, size_t n);
int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, size_t n);
char* strcat(char* dest, const char* src);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
char* strrchr(const char* s, int c);
char* strstr(const char* haystack, const char* needle);
void* memcpy(void* dest, const void* src, size_t n);
void* memmove(void* dest, const void* src, size_t n);
void* memset(void* dest, int val, size_t n);
int memcmp(const void* a, const void* b, size_t n);

#ifdef __cplusplus
}
#endif

#endif

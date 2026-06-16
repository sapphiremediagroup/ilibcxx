#ifndef LIBC_STDIO_H
#define LIBC_STDIO_H

#include <stddef.h>
#include <stdarg.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#define LIBCXX_NOEXCEPT noexcept
#else
#define LIBCXX_NOEXCEPT
#endif

typedef struct FILE {
    int fd;
    int eof;
    int error;
} FILE;

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

#ifndef EOF
#define EOF (-1)
#endif

enum {
    SEEK_SET = 0,
    SEEK_CUR = 1,
    SEEK_END = 2
};

int vsnprintf(char* buffer, size_t size, const char* format, va_list args) LIBCXX_NOEXCEPT;
int snprintf(char* buffer, size_t size, const char* format, ...) LIBCXX_NOEXCEPT;
int vsprintf(char* buffer, const char* format, va_list args) LIBCXX_NOEXCEPT;
int sprintf(char* buffer, const char* format, ...) LIBCXX_NOEXCEPT;
int sscanf(const char* str, const char* format, ...);
int printf(const char* format, ...) LIBCXX_NOEXCEPT;
int vfprintf(FILE* stream, const char* format, va_list args) LIBCXX_NOEXCEPT;
int fprintf(FILE* stream, const char* format, ...) LIBCXX_NOEXCEPT;
int fputs(const char* s, FILE* stream);
int fputc(int c, FILE* stream);
int putchar(int c) LIBCXX_NOEXCEPT;
FILE* fopen(const char* path, const char* mode);
FILE* fdopen(int fd, const char* mode);
int fclose(FILE* stream);
size_t fread(void* ptr, size_t size, size_t count, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t count, FILE* stream);
int fseek(FILE* stream, long offset, int whence);
long ftell(FILE* stream);
int fflush(FILE* stream);
int feof(FILE* stream);
int ferror(FILE* stream);
void clearerr(FILE* stream);
int fileno(FILE* stream);
int remove(const char* path);
int rename(const char* oldpath, const char* newpath);

#ifdef __cplusplus
}
#endif

#undef LIBCXX_NOEXCEPT

#endif

#include <cstddef.hpp>
#include <cstdint.hpp>
#include <cstring.hpp>
#include <memory.hpp>
#include <cstdlib.hpp>
#include <cstdio.hpp>
#include <cmath.hpp>
#include <cstdarg.hpp>
#include <stdarg.h>
#include <syscall.hpp>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <io.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>

static FILE __stdin_file = { 0, 0, 0 };
static FILE __stdout_file = { 1, 0, 0 };
static FILE __stderr_file = { 2, 0, 0 };

extern "C" int _fltused = 0;
extern "C" void __chkstk() {}

extern "C" int errno = 0;
extern "C" FILE* stdin = &__stdin_file;
extern "C" FILE* stdout = &__stdout_file;
extern "C" FILE* stderr = &__stderr_file;

static unsigned long __rand_state = 1;

static inline float fast_atanf(float x) {
    const float pi = 3.14159265358979323846f;
    float ax = x < 0.0f ? -x : x;
    return (pi * 0.25f) * x - x * (ax - 1.0f) * (0.2447f + 0.0663f * ax);
}

extern "C" {

void* malloc(size_t size) noexcept {
    return std::malloc(size);
}

void free(void* ptr) noexcept {
    std::free(ptr);
}

void* realloc(void* ptr, size_t size) noexcept {
    return std::realloc(ptr, size);
}

void* calloc(size_t nmemb, size_t size) noexcept {
    return std::calloc(nmemb, size);
}

int open(const char* path, int flags, ...) {
    if (path == nullptr) {
        errno = EINVAL;
        return -1;
    }
    const auto result = std::open(path, static_cast<std::uint64_t>(flags), 0);
    if (result == static_cast<std::uint64_t>(-1)) {
        errno = ENOENT;
        return -1;
    }
    return static_cast<int>(result);
}

ssize_t read(int fd, void* buf, size_t count) {
    if (buf == nullptr && count != 0) {
        errno = EINVAL;
        return -1;
    }
    const auto result = std::read(static_cast<std::uint64_t>(fd), buf, count);
    if (result == static_cast<std::uint64_t>(-1)) {
        errno = EBADF;
        return -1;
    }
    return static_cast<ssize_t>(result);
}

ssize_t write(int fd, const void* buf, size_t count) {
    if (buf == nullptr && count != 0) {
        errno = EINVAL;
        return -1;
    }
    const auto result = std::write(static_cast<std::uint64_t>(fd), buf, count);
    if (result == static_cast<std::uint64_t>(-1)) {
        errno = EBADF;
        return -1;
    }
    return static_cast<ssize_t>(result);
}

int close(int fd) {
    const auto result = std::close(static_cast<std::uint64_t>(fd));
    if (result == static_cast<std::uint64_t>(-1)) {
        errno = EBADF;
        return -1;
    }
    return 0;
}

off_t lseek(int fd, off_t offset, int whence) {
    const auto result = std::seek(static_cast<std::uint64_t>(fd), static_cast<std::int64_t>(offset),
        static_cast<std::uint64_t>(whence));
    if (result == static_cast<std::uint64_t>(-1)) {
        errno = ESPIPE;
        return static_cast<off_t>(-1);
    }
    return static_cast<off_t>(result);
}

int chdir(const char* path) {
    if (path == nullptr) {
        errno = EINVAL;
        return -1;
    }
    const auto result = std::chdir(path);
    if (result == static_cast<std::uint64_t>(-1)) {
        errno = ENOENT;
        return -1;
    }
    return 0;
}

char* getcwd(char* buf, size_t size) {
    if (buf == nullptr || size == 0) {
        errno = EINVAL;
        return nullptr;
    }
    char* result = std::getcwd(buf, size);
    if (result == nullptr || reinterpret_cast<std::uint64_t>(result) == static_cast<std::uint64_t>(-1)) {
        errno = EINVAL;
        return nullptr;
    }
    return result;
}

int unlink(const char* path) {
    if (path == nullptr) {
        errno = EINVAL;
        return -1;
    }
    const auto result = std::unlink(path);
    if (result == static_cast<std::uint64_t>(-1)) {
        errno = ENOENT;
        return -1;
    }
    return 0;
}

size_t strlen(const char* s) noexcept { return std::strlen(s); }
void* memchr(const void* s, int c, size_t n) noexcept {
    const unsigned char* bytes = static_cast<const unsigned char*>(s);
    const unsigned char needle = static_cast<unsigned char>(c);
    for (size_t i = 0; i < n; ++i) {
        if (bytes[i] == needle) {
            return const_cast<unsigned char*>(bytes + i);
        }
    }
    return nullptr;
}
int strcmp(const char* a, const char* b) noexcept { return std::strcmp(a, b); }
int strncmp(const char* a, const char* b, size_t n) noexcept { return std::strncmp(a, b, n); }
char* strcat(char* dest, const char* src) noexcept {
    char* out = dest + std::strlen(dest);
    while (*src != '\0') {
        *out++ = *src++;
    }
    *out = '\0';
    return dest;
}
char* strcpy(char* dest, const char* src) noexcept { return std::strcpy(dest, src); }
char* strncpy(char* dest, const char* src, size_t n) noexcept { return std::strncpy(dest, src, n); }
char* strrchr(const char* s, int c) noexcept {
    const char needle = static_cast<char>(c);
    const char* last = nullptr;
    for (;; ++s) {
        if (*s == needle) {
            last = s;
        }
        if (*s == '\0') {
            break;
        }
    }
    return const_cast<char*>(last);
}
char* strstr(const char* haystack, const char* needle) noexcept {
    if (*needle == '\0') {
        return const_cast<char*>(haystack);
    }
    for (const char* h = haystack; *h != '\0'; ++h) {
        const char* hs = h;
        const char* ns = needle;
        while (*hs != '\0' && *ns != '\0' && *hs == *ns) {
            ++hs;
            ++ns;
        }
        if (*ns == '\0') {
            return const_cast<char*>(h);
        }
    }
    return nullptr;
}
void* memcpy(void* dest, const void* src, size_t n) noexcept { return std::memcpy(dest, src, n); }
void* memmove(void* dest, const void* src, size_t n) noexcept { return std::memmove(dest, src, n); }
void* memset(void* dest, int val, size_t n) noexcept { return std::memset(dest, val, n); }
int memcmp(const void* a, const void* b, size_t n) noexcept { return std::memcmp(a, b, n); }

long strtol(const char* nptr, char** endptr, int base) {
    if (nptr == nullptr) {
        if (endptr) {
            *endptr = nullptr;
        }
        errno = EINVAL;
        return 0;
    }

    const char* p = nptr;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') {
        ++p;
    }

    int sign = 1;
    if (*p == '+' || *p == '-') {
        sign = (*p == '-') ? -1 : 1;
        ++p;
    }

    if (base == 0) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            base = 16;
            p += 2;
        } else if (p[0] == '0') {
            base = 8;
            ++p;
        } else {
            base = 10;
        }
    } else if (base == 16 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
    }

    if (base < 2 || base > 36) {
        if (endptr) {
            *endptr = const_cast<char*>(nptr);
        }
        errno = EINVAL;
        return 0;
    }

    unsigned long value = 0;
    bool any = false;
    for (;; ++p) {
        int digit;
        if (*p >= '0' && *p <= '9') {
            digit = *p - '0';
        } else if (*p >= 'a' && *p <= 'z') {
            digit = *p - 'a' + 10;
        } else if (*p >= 'A' && *p <= 'Z') {
            digit = *p - 'A' + 10;
        } else {
            break;
        }

        if (digit >= base) {
            break;
        }

        any = true;
        value = value * static_cast<unsigned long>(base) + static_cast<unsigned long>(digit);
    }

    if (endptr) {
        *endptr = const_cast<char*>(any ? p : nptr);
    }

    if (!any) {
        return 0;
    }

    if (sign < 0) {
        const unsigned long limit = static_cast<unsigned long>(LONG_MAX) + 1UL;
        if (value > limit) {
            errno = ERANGE;
            return LONG_MIN;
        }
        if (value == limit) {
            return LONG_MIN;
        }
        return -static_cast<long>(value);
    }

    if (value > static_cast<unsigned long>(LONG_MAX)) {
        errno = ERANGE;
        return LONG_MAX;
    }

    return static_cast<long>(value);
}

char* getenv(const char* name) {
    (void)name;
    return nullptr;
}

static void qsort_swap(unsigned char* a, unsigned char* b, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        const unsigned char tmp = a[i];
        a[i] = b[i];
        b[i] = tmp;
    }
}

static void qsort_impl(unsigned char* base, long left, long right, size_t size,
                       int (*compar)(const void*, const void*)) {
    if (left >= right) {
        return;
    }

    const long pivot_index = left + (right - left) / 2;
    unsigned char* pivot = base + static_cast<size_t>(pivot_index) * size;
    qsort_swap(pivot, base + static_cast<size_t>(right) * size, size);

    long store = left;
    for (long i = left; i < right; ++i) {
        unsigned char* current = base + static_cast<size_t>(i) * size;
        if (compar(current, base + static_cast<size_t>(right) * size) < 0) {
            qsort_swap(base + static_cast<size_t>(store) * size, current, size);
            ++store;
        }
    }

    qsort_swap(base + static_cast<size_t>(store) * size, base + static_cast<size_t>(right) * size, size);
    qsort_impl(base, left, store - 1, size, compar);
    qsort_impl(base, store + 1, right, size, compar);
}

void qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*)) {
    if (base == nullptr || compar == nullptr || nmemb < 2 || size == 0) {
        return;
    }
    qsort_impl(static_cast<unsigned char*>(base), 0, static_cast<long>(nmemb - 1), size, compar);
}

int vsnprintf(char* buffer, size_t size, const char* format, va_list args) noexcept {
    return std::vsnprintf(buffer, size, format, args);
}

int snprintf(char* buffer, size_t size, const char* format, ...) noexcept {
    va_list args;
    va_start(args, format);
    int r = std::vsnprintf(buffer, size, format, args);
    va_end(args);
    return r;
}

int printf(const char* format, ...) noexcept {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    int written = std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (written > 0) {
        std::size_t count = static_cast<std::size_t>(written);
        if (count >= sizeof(buffer)) {
            count = sizeof(buffer) - 1;
        }
        std::write(std::STDOUT_HANDLE, buffer, count);
    }
    return written;
}

int putchar(int c) noexcept {
    const char ch = static_cast<char>(c);
    return static_cast<int>(std::write(std::STDOUT_HANDLE, &ch, 1));
}

FILE* fopen(const char* path, const char* mode) {
    if (path == nullptr || mode == nullptr || mode[0] == '\0') {
        errno = EINVAL;
        return nullptr;
    }

    int flags = O_RDONLY;
    switch (mode[0]) {
        case 'r':
            flags = O_RDONLY;
            break;
        case 'w':
            flags = O_WRONLY;
            break;
        case 'a':
            flags = O_WRONLY;
            break;
        default:
            errno = EINVAL;
            return nullptr;
    }

    bool plus = false;
    for (const char* p = mode + 1; *p != '\0'; ++p) {
        if (*p == '+') {
            plus = true;
            break;
        }
    }

    if (plus) {
        flags = O_RDWR;
    }

    FILE* stream = static_cast<FILE*>(std::malloc(sizeof(FILE)));
    if (stream == nullptr) {
        errno = ENOMEM;
        return nullptr;
    }

    const int fd = open(path, flags);
    if (fd < 0) {
        std::free(stream);
        return nullptr;
    }

    stream->fd = fd;
    stream->eof = 0;
    stream->error = 0;

    if (mode[0] == 'a') {
        if (lseek(fd, 0, SEEK_END) < 0) {
            close(fd);
            std::free(stream);
            return nullptr;
        }
    }

    return stream;
}

int fclose(FILE* stream) {
    if (stream == nullptr) {
        errno = EINVAL;
        return -1;
    }

    const int result = close(stream->fd);
    if (stream != stdin && stream != stdout && stream != stderr) {
        std::free(stream);
    } else {
        stream->eof = 0;
        stream->error = 0;
    }
    return result;
}

size_t fread(void* ptr, size_t size, size_t count, FILE* stream) {
    if (stream == nullptr || (ptr == nullptr && size != 0 && count != 0)) {
        errno = EINVAL;
        return 0;
    }
    if (size == 0 || count == 0) {
        return 0;
    }

    const size_t total = size * count;
    const ssize_t result = read(stream->fd, ptr, total);
    if (result < 0) {
        stream->error = 1;
        return 0;
    }
    if (result == 0) {
        stream->eof = 1;
        return 0;
    }
    if (static_cast<size_t>(result) < total) {
        stream->eof = 1;
    }
    return static_cast<size_t>(result) / size;
}

size_t fwrite(const void* ptr, size_t size, size_t count, FILE* stream) {
    if (stream == nullptr || (ptr == nullptr && size != 0 && count != 0)) {
        errno = EINVAL;
        return 0;
    }
    if (size == 0 || count == 0) {
        return 0;
    }

    const size_t total = size * count;
    const ssize_t result = write(stream->fd, ptr, total);
    if (result < 0) {
        stream->error = 1;
        return 0;
    }
    return static_cast<size_t>(result) / size;
}

int fseek(FILE* stream, long offset, int whence) {
    if (stream == nullptr) {
        errno = EINVAL;
        return -1;
    }
    if (lseek(stream->fd, static_cast<off_t>(offset), whence) < 0) {
        stream->error = 1;
        return -1;
    }
    stream->eof = 0;
    return 0;
}

long ftell(FILE* stream) {
    if (stream == nullptr) {
        errno = EINVAL;
        return -1L;
    }
    const off_t result = lseek(stream->fd, 0, SEEK_CUR);
    if (result < 0) {
        stream->error = 1;
        return -1L;
    }
    return static_cast<long>(result);
}

int fflush(FILE* stream) {
    if (stream == nullptr) {
        return 0;
    }
    return stream->error ? -1 : 0;
}

int feof(FILE* stream) {
    return stream ? stream->eof : 0;
}

int ferror(FILE* stream) {
    return stream ? stream->error : 0;
}

void clearerr(FILE* stream) {
    if (stream) {
        stream->eof = 0;
        stream->error = 0;
    }
}

int fileno(FILE* stream) {
    if (stream == nullptr) {
        errno = EINVAL;
        return -1;
    }
    return stream->fd;
}

int remove(const char* path) {
    return unlink(path);
}

double sqrt(double x) noexcept { return std::sqrt(x); }
double fabs(double x) noexcept { return std::fabs(x); }
double sin(double x) noexcept { return std::sin(x); }
double cos(double x) noexcept { return std::cos(x); }
double tan(double x) noexcept { return std::tan(x); }
double floor(double x) noexcept { return std::floor(x); }
double ceil(double x) noexcept { return std::ceil(x); }

float sqrtf(float x) { return static_cast<float>(std::sqrt(static_cast<double>(x))); }
float fabsf(float x) { return static_cast<float>(std::fabs(static_cast<double>(x))); }

float sinf(float x) { return static_cast<float>(std::sin(static_cast<double>(x))); }
float cosf(float x) { return static_cast<float>(std::cos(static_cast<double>(x))); }
float floorf(float x) { return static_cast<float>(std::floor(static_cast<double>(x))); }

float fminf(float a, float b) { return a < b ? a : b; }

float atan2f(float y, float x);

float asinf(float x) {
    if (x < -1.0f || x > 1.0f) {
        return 0.0f / 0.0f;
    }
    return atan2f(x, sqrtf(1.0f - x * x));
}

float atan2f(float y, float x) {
    const float pi = 3.14159265358979323846f;
    if (x > 0.0f) {
        return fast_atanf(y / x);
    } else if (x < 0.0f) {
        if (y >= 0.0f) return fast_atanf(y / x) + pi;
        return fast_atanf(y / x) - pi;
    } else {
        if (y > 0.0f) return pi * 0.5f;
        if (y < 0.0f) return -pi * 0.5f;
        return 0.0f;
    }
}

float fmaxf(float a, float b) {
    return a > b ? a : b;
}

float acosf(float x) {
    const float pi = 3.14159265358979323846f;
    return (pi * 0.5f) - asinf(x);
}

int stat(const char* path, struct stat* buf) {
    if (path == nullptr || buf == nullptr) return -1;
    auto res = _syscall_impl(static_cast<std::uint64_t>(std::Syscall::Stat),
                             reinterpret_cast<std::uint64_t>(path),
                             reinterpret_cast<std::uint64_t>(buf));
    return static_cast<int>(res);
}

int mkdir(const char* path, mode_t mode) {
    if (path == nullptr) return -1;
    auto res = _syscall_impl(static_cast<std::uint64_t>(std::Syscall::Mkdir),
                             reinterpret_cast<std::uint64_t>(path),
                             static_cast<std::uint64_t>(mode));
    return static_cast<int>(res);
}

int abs(int x) noexcept { return std::abs(x); }

int rand(void) noexcept {
    __rand_state = __rand_state * 1103515245u + 12345u;
    return static_cast<int>((__rand_state >> 16) & 0x7FFFFFFF);
}

void srand(unsigned int seed) noexcept {
    __rand_state = static_cast<unsigned long>(seed);
}

struct find_state {
    char dir[256];
    char pattern[256];
    std::DirEntry* entries;
    std::size_t count;
    std::size_t index;
};

static bool wildcard_match(const char* pat, const char* s) {
    if (!pat || !s) return false;
    while (*pat) {
        if (*pat == '*') {
            ++pat;
            if (!*pat) return true;
            while (*s) {
                if (wildcard_match(pat, s)) return true;
                ++s;
            }
            return false;
        } else if (*pat == '?') {
            if (!*s) return false;
            ++pat; ++s;
        } else {
            if (*pat != *s) return false;
            ++pat; ++s;
        }
    }
    return *s == '\0';
}

_find_t _findfirst(const char* filespec, struct _finddata_t* fileinfo) {
    if (filespec == nullptr || fileinfo == nullptr) {
        errno = EINVAL;
        return -1;
    }

    const char* last_slash = nullptr;
    for (const char* p = filespec; *p; ++p) if (*p == '/') last_slash = p;

    char dir[256] = {0};
    char pattern[256] = {0};
    if (last_slash) {
        std::size_t dlen = static_cast<std::size_t>(last_slash - filespec);
        if (dlen >= sizeof(dir)) dlen = sizeof(dir) - 1;
        for (std::size_t i = 0; i < dlen; ++i) dir[i] = filespec[i];
        const char* patstart = last_slash + 1;
        std::size_t plen = 0;
        while (patstart[plen] && plen + 1 < sizeof(pattern)) { pattern[plen] = patstart[plen]; ++plen; }
    } else {
        dir[0] = '.'; dir[1] = '\0';
        std::size_t plen = 0;
        while (filespec[plen] && plen + 1 < sizeof(pattern)) { pattern[plen] = filespec[plen]; ++plen; }
    }

    find_state* state = static_cast<find_state*>(std::malloc(sizeof(find_state)));
    if (!state) {
        errno = ENOMEM;
        return -1;
    }
    for (std::size_t i = 0; i < sizeof(state->dir); ++i) state->dir[i] = dir[i];
    for (std::size_t i = 0; i < sizeof(state->pattern); ++i) state->pattern[i] = pattern[i];
    state->entries = nullptr;
    state->count = 0;
    state->index = 0;

    const std::size_t cap = 1024;
    state->entries = static_cast<std::DirEntry*>(std::malloc(sizeof(std::DirEntry) * cap));
    if (!state->entries) {
        std::free(state);
        errno = ENOMEM;
        return -1;
    }

    std::uint64_t found = std::readdir(state->dir, state->entries, cap);
    if (found == static_cast<std::uint64_t>(-1)) {
        std::free(state->entries);
        std::free(state);
        errno = ENOENT;
        return -1;
    }

    state->count = static_cast<std::size_t>(found);

    while (state->index < state->count) {
        const auto& e = state->entries[state->index];
        if (wildcard_match(state->pattern, e.name)) {
            for (std::size_t i = 0; i < sizeof(fileinfo->name) - 1 && e.name[i]; ++i) fileinfo->name[i] = e.name[i];
            fileinfo->name[sizeof(fileinfo->name) - 1] = '\0';
            fileinfo->attrib = (e.type == std::FileType::Directory) ? 0x10 : 0x0;
            fileinfo->size_low = 0;
            fileinfo->size_high = 0;
            state->index++;
            return reinterpret_cast<_find_t>(state);
        }
        state->index++;
    }

    std::free(state->entries);
    std::free(state);
    errno = ENOENT;
    return -1;
}

int _findnext(_find_t handle, struct _finddata_t* fileinfo) {
    if (handle == -1 || fileinfo == nullptr) {
        errno = EINVAL;
        return -1;
    }
    find_state* state = reinterpret_cast<find_state*>(handle);
    while (state->index < state->count) {
        const auto& e = state->entries[state->index];
        if (wildcard_match(state->pattern, e.name)) {
            for (std::size_t i = 0; i < sizeof(fileinfo->name) - 1 && e.name[i]; ++i) fileinfo->name[i] = e.name[i];
            fileinfo->name[sizeof(fileinfo->name) - 1] = '\0';
            fileinfo->attrib = (e.type == std::FileType::Directory) ? 0x10 : 0x0;
            fileinfo->size_low = 0;
            fileinfo->size_high = 0;
            state->index++;
            return 0;
        }
        state->index++;
    }
    errno = ENOENT;
    return -1;
}

int _findclose(_find_t handle) {
    if (handle == -1) {
        errno = EINVAL;
        return -1;
    }
    find_state* state = reinterpret_cast<find_state*>(handle);
    if (state->entries) std::free(state->entries);
    std::free(state);
    return 0;
}

int _getch(void) {
    unsigned char c = 0;
    auto ret = std::read(0, &c, 1);
    if (ret == static_cast<std::uint64_t>(-1)) {
        errno = EINVAL;
        return -1;
    }
    return static_cast<int>(c);
}

int _chdir(const char* path) {
    if (path == nullptr) {
        errno = EINVAL;
        return -1;
    }
    auto res = std::chdir(path);
    if (res == static_cast<std::uint64_t>(-1)) {
        errno = ENOENT;
        return -1;
    }
    return 0;
}

int _mkdir(const char* path) {
    if (path == nullptr) {
        errno = EINVAL;
        return -1;
    }
    auto res = std::mkdir(path, 0755);
    if (res == static_cast<std::uint64_t>(-1)) {
        errno = EEXIST;
        return -1;
    }
    return 0;
}

int getch(void) {
    return _getch();
}

int kbhit(void) {
    return 0;
}

void exit(int status) {
    std::exit(static_cast<std::uint64_t>(status));
}

void abort(void) {
    std::exit(1);
}

void __stack_chk_fail(void) {
    abort();
}

void __assert_fail(const char* expr, const char* file, int line, const char* func) {
    char buf[256];
    int n = std::snprintf(buf, sizeof(buf), "Assertion failed: %s, function %s, file %s, line %d\n",
                         expr ? expr : "(null)", func ? func : "(null)", file ? file : "(null)", line);
    if (n > 0) {
        std::write(2, buf, static_cast<std::uint64_t>(n));
    }
    std::exit(1);
}

time_t time(time_t* t) {
    uint64_t ms = std::gettime();
    time_t sec = static_cast<time_t>(ms / 1000);
    if (t) *t = sec;
    return sec;
}

int clock_gettime(int clk_id, struct timespec* tp) {
    if (tp == nullptr) return -1;
    uint64_t ms = std::gettime();
    tp->tv_sec = static_cast<time_t>(ms / 1000);
    tp->tv_nsec = static_cast<long>((ms % 1000) * 1000000);
    return 0;
}

}

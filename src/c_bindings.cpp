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
#include <errno.h>
#include <io.h>

extern "C" int _fltused = 0;

extern "C" int errno = 0;

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

size_t strlen(const char* s) noexcept { return std::strlen(s); }
int strcmp(const char* a, const char* b) noexcept { return std::strcmp(a, b); }
int strncmp(const char* a, const char* b, size_t n) noexcept { return std::strncmp(a, b, n); }
char* strcpy(char* dest, const char* src) noexcept { return std::strcpy(dest, src); }
char* strncpy(char* dest, const char* src, size_t n) noexcept { return std::strncpy(dest, src, n); }
void* memcpy(void* dest, const void* src, size_t n) noexcept { return std::memcpy(dest, src, n); }
void* memmove(void* dest, const void* src, size_t n) noexcept { return std::memmove(dest, src, n); }
void* memset(void* dest, int val, size_t n) noexcept { return std::memset(dest, val, n); }
int memcmp(const void* a, const void* b, size_t n) noexcept { return std::memcmp(a, b, n); }

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

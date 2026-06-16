#ifndef LIBC_TIME_H
#define LIBC_TIME_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t time_t;

struct timespec {
    time_t tv_sec; /* seconds */
    long tv_nsec;  /* nanoseconds */
};

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

typedef int clockid_t;

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1

int clock_gettime(clockid_t clk_id, struct timespec* tp);
time_t time(time_t* t);
struct tm* localtime(const time_t* timer);
int utimensat(int dirfd, const char* pathname, const struct timespec times[2], int flags);

#ifdef __cplusplus
}
#endif

#endif

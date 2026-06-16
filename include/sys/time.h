#ifndef LIBC_SYS_TIME_H
#define LIBC_SYS_TIME_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LIBC_STRUCT_TIMEVAL_DEFINED
#define LIBC_STRUCT_TIMEVAL_DEFINED
struct timeval {
    long tv_sec;
    long tv_usec;
};
#endif

struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

int gettimeofday(struct timeval* tv, struct timezone* tz);

#ifdef __cplusplus
}
#endif

#endif

#ifndef LIBC_SYS_SELECT_H
#define LIBC_SYS_SELECT_H

#include <sys/types.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FD_SETSIZE 1024

typedef struct {
    unsigned long fds_bits[FD_SETSIZE / (8 * sizeof(unsigned long))];
} fd_set;

#define FD_ZERO(set) do { \
    for (int __i = 0; __i < (int)(FD_SETSIZE / (8 * sizeof(unsigned long))); ++__i) { \
        (set)->fds_bits[__i] = 0; \
    } \
} while (0)

#define FD_SET(fd, set) do { \
    if ((fd) >= 0 && (fd) < FD_SETSIZE) { \
        (set)->fds_bits[(fd) / (8 * sizeof(unsigned long))] |= \
            (1UL << ((fd) % (8 * sizeof(unsigned long)))); \
    } \
} while (0)

#define FD_CLR(fd, set) do { \
    if ((fd) >= 0 && (fd) < FD_SETSIZE) { \
        (set)->fds_bits[(fd) / (8 * sizeof(unsigned long))] &= \
            ~(1UL << ((fd) % (8 * sizeof(unsigned long)))); \
    } \
} while (0)

#define FD_ISSET(fd, set) \
    (((fd) >= 0 && (fd) < FD_SETSIZE) ? \
        (((set)->fds_bits[(fd) / (8 * sizeof(unsigned long))] & \
            (1UL << ((fd) % (8 * sizeof(unsigned long))))) != 0) : 0)

int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds,
           struct timeval* timeout);

#ifdef __cplusplus
}
#endif

#endif

#ifndef LIBC_ERRNO_H
#define LIBC_ERRNO_H

#ifdef __cplusplus
extern "C" {
#endif

int* __errno_location(void);
#define errno (*__errno_location())

#define ENOENT 2
#define ESRCH 3
#define EINTR 4
#define EAGAIN 11
#define EBADF 9
#define ECHILD 10
#define EIO 5
#define ENXIO 6
#define EFAULT 14
#define EINVAL 22
#define ENOMEM 12
#define EACCES 13
#define EPERM 1
#define EADDRINUSE 98
#define EADDRNOTAVAIL 99
#define ECONNREFUSED 111
#define ECONNRESET 104
#define EBUSY 16
#define EEXIST 17
#define EXDEV 18
#define ENOTDIR 20
#define EISDIR 21
#define EDEADLK 35
#define ESPIPE 29
#define ENFILE 23
#define EMFILE 24
#define ENOSPC 28
#define EPIPE 32
#define ERANGE 34
#define ENAMETOOLONG 36
#define ENOSYS 38
#define ENOTEMPTY 39
#define ELOOP 40
#define ENOTTY 25
#define ENOTSOCK 88
#define ENOPROTOOPT 92
#define EOPNOTSUPP 95
#define EAFNOSUPPORT 97
#define EPROTONOSUPPORT 93
#define ETIMEDOUT 110
#define ENOTCONN 107
#define EOVERFLOW 75

#ifdef __cplusplus
}
#endif

#endif

#ifndef LIBC_SYS_TYPES_H
#define LIBC_SYS_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef long ssize_t;
typedef long off_t;
typedef uint32_t mode_t;
typedef int pid_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef unsigned long ino_t;
typedef long blksize_t;
typedef long blkcnt_t;
typedef unsigned long dev_t;
typedef unsigned int nlink_t;
typedef unsigned int socklen_t;

#ifdef __cplusplus
}
#endif

#endif

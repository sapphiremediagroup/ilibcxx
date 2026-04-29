#ifndef LIBC_FCNTL_H
#define LIBC_FCNTL_H

#ifdef __cplusplus
extern "C" {
#endif

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_ACCMODE 3

int open(const char* path, int flags, ...);

#ifdef __cplusplus
}
#endif

#endif

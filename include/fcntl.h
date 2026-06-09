#ifndef LIBC_FCNTL_H
#define LIBC_FCNTL_H

#ifdef __cplusplus
extern "C" {
#endif

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_ACCMODE 3
#define O_CREAT 0100
#define O_EXCL 0200
#define O_TRUNC 01000
#define O_APPEND 02000
#define O_NONBLOCK 04000
#define O_DIRECTORY 0200000
#define O_NOFOLLOW 0400000
#define O_CLOEXEC 02000000

#define F_GETFD 1
#define F_SETFD 2

#define FD_CLOEXEC 1

#define AT_FDCWD -100
#define AT_SYMLINK_NOFOLLOW 0x100

int open(const char* path, int flags, ...);
int fcntl(int fd, int cmd, ...);

#ifdef __cplusplus
}
#endif

#endif

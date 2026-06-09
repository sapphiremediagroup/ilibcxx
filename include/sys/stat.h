#ifndef LIBC_SYS_STAT_H
#define LIBC_SYS_STAT_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

struct stat {
    uint64_t st_dev;
    uint64_t st_ino;
    uint32_t st_mode;
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint64_t st_rdev;
    uint64_t st_size;
    uint64_t st_blksize;
    uint64_t st_blocks;
    uint64_t st_atime;
    uint64_t st_mtime;
    uint64_t st_ctime;
};

int stat(const char* path, struct stat* buf);
int lstat(const char* path, struct stat* buf);
int fstat(int fd, struct stat* buf);
int mkdir(const char* path, mode_t mode);
int unlink(const char* path);
int chmod(const char* path, mode_t mode);
int fchmod(int fd, mode_t mode);
int futimens(int fd, const struct timespec times[2]);
mode_t umask(mode_t mask);

#define UTIME_NOW 1073741823L
#define UTIME_OMIT 1073741822L

#define S_IFMT  0170000
#define S_IFDIR 0040000
#define S_IFCHR 0020000
#define S_IFLNK 0120000
#define S_IFREG 0100000
#define S_IFIFO 0010000

#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRGRP 0040
#define S_IWGRP 0020
#define S_IXGRP 0010
#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IXOTH 0001

#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#define S_ISCHR(mode) (((mode) & S_IFMT) == S_IFCHR)
#define S_ISLNK(mode) (((mode) & S_IFMT) == S_IFLNK)
#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#define S_ISFIFO(mode) (((mode) & S_IFMT) == S_IFIFO)

#ifdef __cplusplus
}
#endif

#endif

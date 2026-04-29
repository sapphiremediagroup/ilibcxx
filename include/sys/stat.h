#ifndef LIBC_SYS_STAT_H
#define LIBC_SYS_STAT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t mode_t;

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
int mkdir(const char* path, mode_t mode);
int unlink(const char* path);

#ifdef __cplusplus
}
#endif

#endif

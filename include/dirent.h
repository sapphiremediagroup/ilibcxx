#ifndef LIBC_DIRENT_H
#define LIBC_DIRENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DIR DIR;

struct dirent {
    uint64_t d_ino;
    unsigned char d_type;
    char d_name[256];
};

#define DT_UNKNOWN 0
#define DT_REG 8
#define DT_DIR 4

DIR* opendir(const char* name);
struct dirent* readdir(DIR* dirp);
int closedir(DIR* dirp);
void rewinddir(DIR* dirp);
long telldir(DIR* dirp);
void seekdir(DIR* dirp, long loc);

#ifdef __cplusplus
}
#endif

#endif

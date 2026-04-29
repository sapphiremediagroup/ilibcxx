#ifndef LIBC_DIRECT_H
#define LIBC_DIRECT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Minimal direct.h compatibility layer */
int _getch(void);
int _chdir(const char* path);
int _mkdir(const char* path);

#ifdef __cplusplus
}
#endif

#endif

#ifndef LIBC_IO_H
#define LIBC_IO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef long _find_t;

struct _finddata_t {
    char name[256];
    unsigned long attrib;
    unsigned long size_low;
    unsigned long size_high;
};

/* Returns handle >= 0 on success and fills `fileinfo`. Returns -1 on error and sets `errno`. */
_find_t _findfirst(const char* filespec, struct _finddata_t* fileinfo);

/* Returns 0 on success and fills `fileinfo`. Returns -1 on error and sets `errno`. */
int _findnext(_find_t handle, struct _finddata_t* fileinfo);

/* Close find handle. Returns 0 on success, -1 on failure. */
int _findclose(_find_t handle);

#ifdef __cplusplus
}
#endif

#endif

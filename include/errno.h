#ifndef LIBC_ERRNO_H
#define LIBC_ERRNO_H

#ifdef __cplusplus
extern "C" {
#endif

extern int errno;

#define ENOENT 2
#define EBADF 9
#define EIO 5
#define EINVAL 22
#define ENOMEM 12
#define EACCES 13
#define EEXIST 17
#define ESPIPE 29
#define ENFILE 23
#define EMFILE 24
#define ERANGE 34

#ifdef __cplusplus
}
#endif

#endif

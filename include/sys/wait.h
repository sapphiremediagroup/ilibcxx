#ifndef LIBC_SYS_WAIT_H
#define LIBC_SYS_WAIT_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WNOHANG 1

#define WEXITSTATUS(status) (((status) >> 8) & 0xFF)
#define WTERMSIG(status) ((status) & 0x7F)
#define WIFEXITED(status) (WTERMSIG(status) == 0)
#define WIFSIGNALED(status) (WTERMSIG(status) != 0)

pid_t wait(int* status);
pid_t waitpid(pid_t pid, int* status, int options);

#ifdef __cplusplus
}
#endif

#endif

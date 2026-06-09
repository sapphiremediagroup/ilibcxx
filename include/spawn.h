#ifndef LIBC_SPAWN_H
#define LIBC_SPAWN_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int unused;
} posix_spawnattr_t;

typedef struct {
    int unsupported_actions;
} posix_spawn_file_actions_t;

int posix_spawn(pid_t* pid, const char* path,
                const posix_spawn_file_actions_t* file_actions,
                const posix_spawnattr_t* attrp,
                char* const argv[], char* const envp[]);
int posix_spawnp(pid_t* pid, const char* file,
                 const posix_spawn_file_actions_t* file_actions,
                 const posix_spawnattr_t* attrp,
                 char* const argv[], char* const envp[]);

int posix_spawnattr_init(posix_spawnattr_t* attr);
int posix_spawnattr_destroy(posix_spawnattr_t* attr);
int posix_spawn_file_actions_init(posix_spawn_file_actions_t* file_actions);
int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t* file_actions);
int posix_spawn_file_actions_addclose(posix_spawn_file_actions_t* file_actions, int fd);
int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t* file_actions, int oldfd, int newfd);
int posix_spawn_file_actions_addopen(posix_spawn_file_actions_t* file_actions,
                                     int fd, const char* path, int oflag, mode_t mode);

#ifdef __cplusplus
}
#endif

#endif

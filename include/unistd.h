#ifndef LIBC_UNISTD_H
#define LIBC_UNISTD_H

#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

ssize_t read(int fd, void* buf, size_t count);
ssize_t write(int fd, const void* buf, size_t count);
int close(int fd);
int dup(int fd);
int dup2(int oldfd, int newfd);
int pipe(int fildes[2]);
off_t lseek(int fd, off_t offset, int whence);
int chdir(const char* path);
char* getcwd(char* buf, size_t size);
int unlink(const char* path);
int link(const char* oldpath, const char* newpath);
int symlink(const char* target, const char* linkpath);
ssize_t readlink(const char* path, char* buf, size_t bufsiz);
int rmdir(const char* path);
int access(const char* path, int mode);
int truncate(const char* path, off_t length);
int ftruncate(int fd, off_t length);
int fsync(int fd);
int fdatasync(int fd);
pid_t getpid(void);
pid_t getppid(void);
pid_t getpgrp(void);
int setpgid(pid_t pid, pid_t pgid);
pid_t tcgetpgrp(int fd);
int tcsetpgrp(int fd, pid_t pgrp);
int isatty(int fd);
int fork(void);
int execve(const char* path, char* const argv[], char* const envp[]);
unsigned int sleep(unsigned int seconds);

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#ifdef __cplusplus
}
#endif

#endif

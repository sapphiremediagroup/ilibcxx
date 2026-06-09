#ifndef LIBC_SIGNAL_H
#define LIBC_SIGNAL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int sig_atomic_t;
typedef unsigned long sigset_t;
typedef void (*sighandler_t)(int);
#ifndef LIBC_PTHREAD_T_DEFINED
#define LIBC_PTHREAD_T_DEFINED
typedef unsigned long pthread_t;
#endif

typedef struct {
    void* ss_sp;
    size_t ss_size;
    int ss_flags;
} stack_t;

struct sigaction {
    sighandler_t sa_handler;
    sigset_t sa_mask;
    int sa_flags;
};

#define SIG_ERR ((sighandler_t)-1)
#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)

#define SIGHUP  1
#define SIGINT  2
#define SIGQUIT 3
#define SIGILL  4
#define SIGABRT 6
#define SIGFPE  8
#define SIGKILL 9
#define SIGUSR1 10
#define SIGSEGV 11
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGCHLD 17

#define NSIG 32

#define SA_ONSTACK 0x08000000
#define SA_RESTART 0x10000000

#define SS_ONSTACK 1
#define SS_DISABLE 2
#define MINSIGSTKSZ 2048
#define SIGSTKSZ 8192

#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

sighandler_t signal(int sig, sighandler_t handler);
int raise(int sig);
int sigemptyset(sigset_t* set);
int sigfillset(sigset_t* set);
int sigaddset(sigset_t* set, int sig);
int sigdelset(sigset_t* set, int sig);
int sigismember(const sigset_t* set, int sig);
int sigaction(int sig, const struct sigaction* act, struct sigaction* oldact);
int sigprocmask(int how, const sigset_t* set, sigset_t* oldset);
int pthread_sigmask(int how, const sigset_t* set, sigset_t* oldset);
int pthread_kill(pthread_t thread, int sig);
int sigaltstack(const stack_t* ss, stack_t* old_ss);

#ifdef __cplusplus
}
#endif

#endif

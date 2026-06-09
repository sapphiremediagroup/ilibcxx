#ifndef LIBC_PTHREAD_H
#define LIBC_PTHREAD_H

#include <signal.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LIBC_PTHREAD_T_DEFINED
#define LIBC_PTHREAD_T_DEFINED
typedef unsigned long pthread_t;
#endif
typedef unsigned long pthread_key_t;
typedef struct {
    int lock;
    unsigned long owner;
    int recursion;
    int type;
} pthread_mutex_t;
typedef struct {
    int type;
} pthread_mutexattr_t;
typedef struct {
    int unused;
} pthread_attr_t;
typedef struct {
    int waiters;
    int signals;
    int broadcast;
} pthread_cond_t;
typedef struct {
    int unused;
} pthread_condattr_t;
typedef struct {
    int readers;
    int writer;
    int pending_writers;
} pthread_rwlock_t;
typedef struct {
    int unused;
} pthread_rwlockattr_t;

#define PTHREAD_MUTEX_INITIALIZER { 0, 0, 0, PTHREAD_MUTEX_NORMAL }
#define PTHREAD_COND_INITIALIZER { 0, 0, 0 }
#define PTHREAD_RWLOCK_INITIALIZER { 0, 0, 0 }

#define PTHREAD_MUTEX_NORMAL 0
#define PTHREAD_MUTEX_RECURSIVE 1
#define PTHREAD_MUTEX_ERRORCHECK 2
#define PTHREAD_MUTEX_DEFAULT PTHREAD_MUTEX_NORMAL

#define PTHREAD_CANCEL_ENABLE 0
#define PTHREAD_CANCEL_DISABLE 1
#define PTHREAD_CANCEL_DEFERRED 0
#define PTHREAD_CANCEL_ASYNCHRONOUS 1
#define PTHREAD_CANCELED ((void*)-1)

#define PTHREAD_KEYS_MAX 64

int pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*start_routine)(void*), void* arg);
int pthread_join(pthread_t thread, void** retval);
void pthread_exit(void* retval);
pthread_t pthread_self(void);
int pthread_equal(pthread_t a, pthread_t b);
int pthread_detach(pthread_t thread);

int pthread_key_create(pthread_key_t* key, void (*destructor)(void*));
int pthread_key_delete(pthread_key_t key);
void* pthread_getspecific(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, const void* value);

int pthread_cancel(pthread_t thread);
void pthread_testcancel(void);
int pthread_setcancelstate(int state, int* oldstate);
int pthread_setcanceltype(int type, int* oldtype);

int pthread_mutexattr_init(pthread_mutexattr_t* attr);
int pthread_mutexattr_destroy(pthread_mutexattr_t* attr);
int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type);
int pthread_mutexattr_gettype(const pthread_mutexattr_t* attr, int* type);

int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr);
int pthread_mutex_destroy(pthread_mutex_t* mutex);
int pthread_mutex_lock(pthread_mutex_t* mutex);
int pthread_mutex_trylock(pthread_mutex_t* mutex);
int pthread_mutex_unlock(pthread_mutex_t* mutex);

int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr);
int pthread_cond_destroy(pthread_cond_t* cond);
int pthread_cond_signal(pthread_cond_t* cond);
int pthread_cond_broadcast(pthread_cond_t* cond);
int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex);
int pthread_cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* mutex, const struct timespec* abstime);

int pthread_rwlock_init(pthread_rwlock_t* rwlock, const pthread_rwlockattr_t* attr);
int pthread_rwlock_destroy(pthread_rwlock_t* rwlock);
int pthread_rwlock_rdlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_tryrdlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_wrlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_trywrlock(pthread_rwlock_t* rwlock);
int pthread_rwlock_unlock(pthread_rwlock_t* rwlock);

#ifdef __cplusplus
}
#endif

#endif

#ifndef __threads_h
#define __threads_h

#if defined(CONFIG_HTXF_PTHREAD)
#include <pthread.h>
#else
#include <sys/types.h>
#include <unistd.h>
#if defined(CONFIG_HTXF_CLONE)
#include <sched.h>
#define CLONE_STACKSIZE	0x10000
#endif
#endif

#if defined(CONFIG_HTXF_PTHREAD)
typedef void * thread_return_type;
typedef pthread_t hxd_thread_t;
typedef pthread_mutex_t hxd_mutex_t;
#else
typedef int thread_return_type;
typedef pid_t hxd_thread_t;
/* spin lock */
typedef int hxd_mutex_t;
#endif

#if defined(CONFIG_HTXF_PTHREAD)
#define mutex_init(_m)		pthread_mutex_init((_m), 0)
#define mutex_lock(_m)		pthread_mutex_lock((_m))
#define mutex_unlock(_m)	pthread_mutex_unlock((_m))
#elif defined(CONFIG_HTXF_CLONE)
#include <sched.h>
#include "spinlock.h"
#define mutex_lock(_m)		do { while (testandset((_m)) sched_yield(); } while (0)
#define mutex_unlock(_m)	do { SL_RELEASE((_m); } while (0)
#define mutex_init(_m)		do { *(_m) = 0; } while (0)
#else
#define mutex_init(_m)
#define mutex_lock(_m)
#define mutex_unlock(_m)
#endif


extern int hxd_thread_create (hxd_thread_t *tidp, void **stackp, thread_return_type (*fn)(void *), void *datap);
extern void mask_signal (int how, int sig);

#endif

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include "threads.h"
#include <signal.h>
#include <errno.h>

int
hxd_thread_create (hxd_thread_t *tidp, void **stackp, thread_return_type (*fn)(void *), void *datap)
{
	int err;
	hxd_thread_t tid;
#if defined(CONFIG_HTXF_CLONE)
	void *stack, *hstack;
#endif

#if defined(CONFIG_HTXF_PTHREAD)
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	err = pthread_create(&tid, &attr, fn, datap);
#else
	mask_signal(SIG_BLOCK, SIGCHLD);
#if defined(CONFIG_HTXF_CLONE)
	/* XXX clone is screwed up in 2.4.20
	   need to pass pointer to middle of stack??? */
	stack = xmalloc(CLONE_STACKSIZE);
	hstack = (void *)(((u_int8_t *)stack + CLONE_STACKSIZE) - sizeof(void *));
	tid = clone(fn, hstack, CLONE_VM|CLONE_FS|CLONE_FILES|SIGCHLD, datap);
#elif defined(CONFIG_HTXF_FORK)
	tid = fork();
	if (tid == 0)
		_exit(fn(datap));
#endif
	if (tid == -1) {
		err = errno;
#if defined(CONFIG_HTXF_CLONE)
		xfree(stack);
#endif
	} else {
		err = 0;
	}
	mask_signal(SIG_UNBLOCK, SIGCHLD);
#endif

	if (!err) {
		*tidp = tid;
#if defined(CONFIG_HTXF_CLONE)
		*stackp = stack;
#else
		*stackp = 0;
#endif
	}

	return err;
}

#if defined(CONFIG_HTXF_CLONE) || defined(CONFIG_HTXF_FORK)
void
mask_signal (int how, int sig)
{
	sigset_t set;

	sigemptyset(&set);
	sigaddset(&set, sig);
	sigprocmask(how, &set, 0);
}
#endif

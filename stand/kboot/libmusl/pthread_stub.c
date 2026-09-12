#include <pthread.h>

/*
 * kboot is single-threaded, so use fixed cancellation state instead of musl's
 * per-thread pthread state.
 */
int
pthread_setcancelstate(int newstate, int *oldstate)
{
	(void)newstate;
	if (oldstate != 0)
		*oldstate = PTHREAD_CANCEL_ENABLE;
	return (0);
}

/* Keep the resolver's socket cleanup active even without cancellation. */
void
_pthread_cleanup_push(struct __ptcb *cb, void (*func)(void *), void *arg)
{
	cb->__f = func;
	cb->__x = arg;
	cb->__next = 0;
}

void
_pthread_cleanup_pop(struct __ptcb *cb, int execute)
{
	if (execute)
		cb->__f(cb->__x);
}

#include "features.h"
#include "lock.h"

/* kboot is single-threaded, so musl's internal spinlocks can be no-ops. */
hidden void
__lock(volatile int *l)
{
	(void)l;
}

hidden void
__unlock(volatile int *l)
{
	(void)l;
}

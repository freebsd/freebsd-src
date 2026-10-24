#include "features.h"
#include "kboot_runtime.h"

/*
 * kboot is single-threaded, so map it to libsa's global errno instead of musl
 * pthread-local state.
 */
hidden int *
___errno_location(void)
{
	return (&errno);
}

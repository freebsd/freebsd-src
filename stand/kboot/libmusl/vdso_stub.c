#include "syscall.h"

/* kboot has no VDSO support, so lead musl time code to raw syscalls. */
hidden void *
__vdsosym(const char *ver, const char *name)
{
	(void)ver;
	(void)name;

	return (0);
}

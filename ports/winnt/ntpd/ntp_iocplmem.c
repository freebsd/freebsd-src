/*
 * ntp_iocplmem.c - separate memory pool for IOCPL related objects
 *
 * Written by Juergen Perlinger (perlinger@ntp.org) for the NTP project.
 * The contents of 'html/copyright.html' apply.
 *
 * --------------------------------------------------------------------
 * Notes on the implementation:
 *
 * Implements a thin layer over Windows Memory pools
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stddef.h>
#include <syslog.h>

#include "ntpd.h"
#include "ntp_iocplmem.h"

/* -------------------------------------------------------------------
 * We make a pool of our own for IO context objects -- the are owned by
 * the system until a completion result is pulled from the queue, and
 * they seriously go into the way of memory tracking until we can safely
 * cancel an IO request.
 * -------------------------------------------------------------------
 */
static	HANDLE	hHeapHandle;

/* -------------------------------------------------------------------
 * Create a new heap for IO context objects
 */
 void
IOCPLPoolInit(
	size_t	initSize
	)
{
	hHeapHandle = HeapCreate(0, initSize, 0);
	if (hHeapHandle == NULL) {
		msyslog(LOG_ERR, "Can't initialize Heap: %m");
		exit(1);
	}
}

/* -------------------------------------------------------------------
 * Delete the IO context heap
 *
 * Since we do not know what callbacks are pending, we just drop the
 * pool into oblivion. New allocs and frees will fail from this moment,
 * but we simply don't care. At least the normal heap dump stats will
 * show no leaks from IO context blocks. On the downside, we have to
 * track them ourselves if something goes wrong.
 */
void
IOCPLPoolDone(void)
{
	hHeapHandle = NULL;
}

/* -------------------------------------------------------------------
 * Alloc & Free on local heap
 *
 * When the heap handle is NULL, these both will fail; Alloc with a NULL
 * return and Free silently.
 */
void * __fastcall
IOCPLPoolAlloc(
	size_t		size,
	const char *	desc
	)
{
	void *	ptr = NULL;

	if (hHeapHandle != NULL)
		ptr = HeapAlloc(hHeapHandle, HEAP_ZERO_MEMORY, max(size, 1));
	if (ptr == NULL)
		errno = ENOMEM;
	DPRINTF(6, ("IOCPLPoolAlloc: '%s', heap=%p, ptr=%p\n",
		desc, hHeapHandle, ptr));
	return ptr;
}
/* ----------------------------------------------------------------- */
void __fastcall
IOCPLPoolFree(
	void *		ptr,
	const char *	desc
	)
{
	DPRINTF(6, ("IOCPLPoolFree: '%s', heap=%p, ptr=%p\n",
		desc, hHeapHandle, ptr));
	if (ptr != NULL && hHeapHandle != NULL)
		HeapFree(hHeapHandle, 0, ptr);
}

/* -------------------------------------------------------------------
 * Allocate a memory buffer and copy the data from a source buffer
 * into the new allocated memory slice.
 */
void * __fastcall
IOCPLPoolMemDup(
const void *	psrc,
size_t		size,
const char *	desc
)
{
	void *	ptr = NULL;

	if (hHeapHandle != NULL) {
		ptr = HeapAlloc(hHeapHandle, 0, max(size, 1));
		if (ptr != NULL)
			memcpy(ptr, psrc, size);
	}
	if (ptr == NULL)
		errno = ENOMEM;

	DPRINTF(6, ("IOCPLPoolMemDup: '%s', heap=%p, ptr=%p\n",
		desc, hHeapHandle, ptr));
	return ptr;

}
/* -*- that's all folks -*- */

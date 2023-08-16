/*
 * This file is in the public domain.
 */

#include <sys/param.h>
#include <dev/zlib/zcalloc.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

MALLOC_DEFINE(M_ZLIB, "zlib", "ZLIB Compressor");

void *
zcalloc_waitok(void *nil, u_int items, u_int size)
{

	return mallocarray(items, size, M_ZLIB, M_WAITOK);
}

void *
zcalloc_nowait(void *nil, u_int items, u_int size)
{

	return mallocarray(items, size, M_ZLIB, M_NOWAIT);
}

void *
zcalloc(void *nil, u_int items, u_int size)
{

	return zcalloc_nowait(nil, items, size);
}

void
zcfree(void *nil, void *ptr)
{

        free(ptr, M_ZLIB);
}

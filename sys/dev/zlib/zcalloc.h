/*
 * This file is in the public domain.
 * $FreeBSD$
 */

#ifndef _DEV_ZLIB_ZCALLOC_
#define _DEV_ZLIB_ZCALLOC_

void * zcalloc_waitok(void *nil, u_int items, u_int size);
void * zcalloc_nowait(void *nil, u_int items, u_int size);
void zcfree(void *nil, void *ptr);

#endif

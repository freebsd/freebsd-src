/*
 * This file is in the public domain.
 * $FreeBSD$
 */

#ifndef _DEV_ZLIB_ZCALLOC_
#define _DEV_ZLIB_ZCALLOC_

#include <contrib/zlib/zutil.h>
#undef local

void *zcalloc_waitok(void *, u_int, u_int);
void *zcalloc_nowait(void *, u_int, u_int);
#endif

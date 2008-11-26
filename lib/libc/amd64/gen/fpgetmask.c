/* $FreeBSD: src/lib/libc/amd64/gen/fpgetmask.c,v 1.1.26.1 2008/10/02 02:57:24 kensmith Exp $ */
#define __IEEEFP_NOINLINES__ 1
#include <ieeefp.h>

fp_except_t fpgetmask(void)
{
	return __fpgetmask();
}

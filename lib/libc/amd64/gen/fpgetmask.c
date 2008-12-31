/* $FreeBSD: src/lib/libc/amd64/gen/fpgetmask.c,v 1.1.28.1 2008/11/25 02:59:29 kensmith Exp $ */
#define __IEEEFP_NOINLINES__ 1
#include <ieeefp.h>

fp_except_t fpgetmask(void)
{
	return __fpgetmask();
}

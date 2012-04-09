/* $FreeBSD: src/lib/libc/amd64/gen/fpsetmask.c,v 1.1.32.1.8.1 2012/03/03 06:15:13 kensmith Exp $ */
#define __IEEEFP_NOINLINES__ 1
#include <ieeefp.h>

fp_except_t fpsetmask(fp_except_t m)
{
	return (__fpsetmask(m));
}

/* $FreeBSD: src/lib/libc/amd64/gen/fpsetmask.c,v 1.1.32.1.4.1 2010/06/14 02:09:06 kensmith Exp $ */
#define __IEEEFP_NOINLINES__ 1
#include <ieeefp.h>

fp_except_t fpsetmask(fp_except_t m)
{
	return (__fpsetmask(m));
}

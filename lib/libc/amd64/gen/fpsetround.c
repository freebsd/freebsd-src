/* $FreeBSD: src/lib/libc/amd64/gen/fpsetround.c,v 1.1.36.1 2010/12/21 17:10:29 kensmith Exp $ */
#define __IEEEFP_NOINLINES__ 1
#include <ieeefp.h>

fp_rnd_t fpsetround(fp_rnd_t m)
{
	return (__fpsetround(m));
}

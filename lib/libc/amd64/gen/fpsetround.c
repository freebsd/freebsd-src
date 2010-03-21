/* $FreeBSD: src/lib/libc/amd64/gen/fpsetround.c,v 1.1.34.1 2010/02/10 00:26:20 kensmith Exp $ */
#define __IEEEFP_NOINLINES__ 1
#include <ieeefp.h>

fp_rnd_t fpsetround(fp_rnd_t m)
{
	return (__fpsetround(m));
}

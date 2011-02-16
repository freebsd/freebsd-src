/* $FreeBSD: src/lib/libc/amd64/gen/fpgetround.c,v 1.1.32.1.6.1 2010/12/21 17:09:25 kensmith Exp $ */
#define __IEEEFP_NOINLINES__ 1
#include <ieeefp.h>

fp_rnd_t fpgetround(void)
{
	return __fpgetround();
}

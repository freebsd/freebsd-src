/* $FreeBSD: src/lib/libc/amd64/gen/fpgetprec.c,v 1.1 2003/07/22 06:46:17 peter Exp $ */
#define __IEEEFP_NOINLINES__ 1
#include <ieeefp.h>

fp_prec_t fpgetprec(void)
{
	return __fpgetprec();
}

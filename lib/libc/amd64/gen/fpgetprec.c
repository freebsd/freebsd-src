/* $FreeBSD: src/lib/libc/amd64/gen/fpgetprec.c,v 1.1.28.1 2008/11/25 02:59:29 kensmith Exp $ */
#define __IEEEFP_NOINLINES__ 1
#include <ieeefp.h>

fp_prec_t fpgetprec(void)
{
	return __fpgetprec();
}

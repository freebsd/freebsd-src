/* $FreeBSD: src/lib/libc/amd64/gen/fpsetprec.c,v 1.1.30.1 2009/04/15 03:14:26 kensmith Exp $ */
#define __IEEEFP_NOINLINES__ 1
#include <ieeefp.h>

fp_prec_t fpsetprec(fp_prec_t m)
{
	return (__fpsetprec(m));
}

/* $FreeBSD: src/lib/libc/amd64/gen/fpsetprec.c,v 1.1.26.1 2008/10/02 02:57:24 kensmith Exp $ */
#define __IEEEFP_NOINLINES__ 1
#include <ieeefp.h>

fp_prec_t fpsetprec(fp_prec_t m)
{
	return (__fpsetprec(m));
}

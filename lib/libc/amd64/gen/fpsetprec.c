/* $FreeBSD: src/lib/libc/amd64/gen/fpsetprec.c,v 1.1.32.1.4.1 2010/06/14 02:09:06 kensmith Exp $ */
#define __IEEEFP_NOINLINES__ 1
#include <ieeefp.h>

fp_prec_t fpsetprec(fp_prec_t m)
{
	return (__fpsetprec(m));
}

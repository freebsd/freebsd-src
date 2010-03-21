/* $FreeBSD: src/lib/libc/amd64/gen/fpgetsticky.c,v 1.1.34.1 2010/02/10 00:26:20 kensmith Exp $ */
#define __IEEEFP_NOINLINES__ 1
#include <ieeefp.h>

fp_except_t fpgetsticky(void)
{
	return __fpgetsticky();
}

/* $FreeBSD: src/lib/libc/amd64/gen/fpgetsticky.c,v 1.1.30.1 2009/04/15 03:14:26 kensmith Exp $ */
#define __IEEEFP_NOINLINES__ 1
#include <ieeefp.h>

fp_except_t fpgetsticky(void)
{
	return __fpgetsticky();
}

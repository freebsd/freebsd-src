/* $FreeBSD$ */
#define __IEEEFP_NOINLINES__ 1
#include <ieeefp.h>

fp_except_t fpsetsticky(fp_except_t m)
{
	return (__fpsetsticky(m));
}

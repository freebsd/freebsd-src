/*
 * This file is in the public domain.  Originally written by Garrett
 * A. Wollman.
 *
 * $FreeBSD$
 */
#include "block_abi.h"
#define I_AM_QSORT_R
#include "qsort.c"

void
qsort_b(void *base, size_t nel, size_t width,
	DECLARE_BLOCK(int, compar, const void *, const void *))
{
	qsort_r(base, nel, width, compar,
		(int (*)(void *, const void *, const void *))
		GET_BLOCK_FUNCTION(compar));
}

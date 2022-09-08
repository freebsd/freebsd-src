/*
 * This file is in the public domain.
 */
#include "block_abi.h"
#define	I_AM_BSORT_R
#include "bsort.c"

typedef DECLARE_BLOCK(int, bsort_block, const void *, const void *);

static int
bsort_b_compare(const void *pa, const void *pb, void *arg)
{
	bsort_block compar;
	int (*cmp)(void *, const void *, const void *);

	compar = arg;
	cmp = (void *)compar->invoke;
	return (cmp(compar, pa, pb));
}

void
bsort_b(void *base, size_t nel, size_t width, bsort_block compar)
{
	bsort_r(base, nel, width, &bsort_b_compare, compar);
}

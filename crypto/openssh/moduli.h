/* $OpenBSD: moduli.h,v 1.1 2003/07/28 09:49:56 djm Exp $ */

#include <sys/types.h>
#include <openssl/bn.h>

/*
 * Using virtual memory can cause thrashing.  This should be the largest
 * number that is supported without a large amount of disk activity --
 * that would increase the run time from hours to days or weeks!
 */
#define LARGE_MINIMUM   (8UL)	/* megabytes */

/*
 * Do not increase this number beyond the unsigned integer bit size.
 * Due to a multiple of 4, it must be LESS than 128 (yielding 2**30 bits).
 */
#define LARGE_MAXIMUM   (127UL)	/* megabytes */

/* Minimum number of primality tests to perform */
#define TRIAL_MINIMUM           (4)

int gen_candidates(FILE *, int, int, BIGNUM *);
int prime_test(FILE *, FILE *, u_int32_t, u_int32_t);

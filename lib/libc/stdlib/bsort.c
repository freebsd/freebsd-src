/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016-2023 Hans Petter Selasky
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/param.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "libc_private.h"

/*
 * A variant of bitonic sorting
 *
 * Worst case sorting complexity: log2(N) * log2(N) * N
 * Additional memory and stack usage: none
 */

#if defined(I_AM_BSORT_R)
typedef int cmp_t (const void *, const void *, void *);
#define	ARG_PROTO void *arg,
#define	ARG_NAME arg ,
#define	CMP(fn,arg,a,b) (fn)(a, b, arg)
#elif defined(I_AM_BSORT_S)
typedef int cmp_t (const void *, const void *, void *);
#define	ARG_PROTO void *arg,
#define	ARG_NAME arg ,
#define	CMP(fn,arg,a,b) (fn)(a, b, arg)
#else
typedef int cmp_t (const void *, const void *);
#define	ARG_PROTO
#define	ARG_NAME
#define	CMP(fn,arg,a,b) (fn)(a, b)
#endif

static inline void
bsort_swap(char *pa, char *pb, const size_t es)
{
	if (__builtin_constant_p(es) && es == 8) {
		uint64_t tmp;

		/* swap */
		tmp = *(uint64_t *)pa;
		*(uint64_t *)pa = *(uint64_t *)pb;
		*(uint64_t *)pb = tmp;
	} else if (__builtin_constant_p(es) && es == 4) {
		uint32_t tmp;

		/* swap */
		tmp = *(uint32_t *)pa;
		*(uint32_t *)pa = *(uint32_t *)pb;
		*(uint32_t *)pb = tmp;
	} else if (__builtin_constant_p(es) && es == 2) {
		uint16_t tmp;

		/* swap */
		tmp = *(uint16_t *)pa;
		*(uint16_t *)pa = *(uint16_t *)pb;
		*(uint16_t *)pb = tmp;
	} else if (__builtin_constant_p(es) && es == 1) {
		uint8_t tmp;

		/* swap */
		tmp = *(uint8_t *)pa;
		*(uint8_t *)pa = *(uint8_t *)pb;
		*(uint8_t *)pb = tmp;
	} else if (es <= 256) {
		char tmp[es] __aligned(8);

		/* swap using fast memcpy() */
		memcpy(tmp, pa, es);
		memcpy(pa, pb, es);
		memcpy(pb, tmp, es);
	} else {
		/* swap byte-by-byte to avoid huge stack usage */
		for (size_t x = 0; x != es; x++) {
			char tmp;

			/* swap */
			tmp = pa[x];
			pa[x] = pb[x];
			pb[x] = tmp;
		}
	}
}

/* The following function returns true when the array is completely sorted. */
static inline bool
bsort_complete(void *ptr, const size_t lim, const size_t es, ARG_PROTO cmp_t *fn)
{
	for (size_t x = 1; x != lim; x++) {
		if (CMP(fn, arg, ptr, (char *)ptr + es) > 0)
			return (false);
		ptr = (char *)ptr + es;
	}
	return (true);
}

static inline void
bsort_xform(char *ptr, const size_t n, size_t lim, const size_t es, ARG_PROTO cmp_t *fn)
{
#define	BSORT_TABLE_MAX (1UL << 4)
	size_t x, y, z;
	unsigned t, u, v;
	size_t p[BSORT_TABLE_MAX];
	char *q[BSORT_TABLE_MAX];

	lim *= es;
	x = n;
	for (;;) {
		/* optimise */
		if (x >= BSORT_TABLE_MAX)
			v = BSORT_TABLE_MAX;
		else if (x >= 2)
			v = x;
		else
			break;

		/* divide down */
		x /= v;

		/* generate ramp table */
		for (t = 0; t != v; t += 2) {
			p[t] = (t * x);
			p[t + 1] = (t + 2) * x - 1;
		}

		/* bitonic sort */
		for (y = 0; y != n; y += (v * x)) {
			for (z = 0; z != x; z++) {
				const size_t w = y + z;

				/* p[0] is always zero and is omitted */
				q[0] = ptr + w * es;

				/* insertion sort */
				for (t = 1; t != v; t++) {
					q[t] = ptr + (w ^ p[t]) * es;

					/* check for array lengths not power of two */
					if ((size_t)(q[t] - ptr) >= lim)
						break;
					for (u = t; u != 0 && CMP(fn, arg, q[u - 1], q[u]) > 0; u--)
						bsort_swap(q[u - 1], q[u], es);
				}
			}
		}
	}
}

static void
local_bsort(void *ptr, const size_t n, const size_t es, ARG_PROTO cmp_t *fn)
{
	size_t max;

	/* if there are less than 2 elements, then sorting is not needed */
	if (__predict_false(n < 2))
		return;

	/* compute power of two, into max */
	if (sizeof(size_t) <= sizeof(unsigned long))
		max = 1UL << (8 * sizeof(unsigned long) - __builtin_clzl(n) - 1);
	else
		max = 1ULL << (8 * sizeof(unsigned long long) - __builtin_clzll(n) - 1);

	/* round up power of two, if needed */
	if (!powerof2(n))
		max <<= 1;

	/* optimize common sort scenarios */
	switch (es) {
	case 8:
		while (!bsort_complete(ptr, n, 8, ARG_NAME fn))
			bsort_xform(ptr, max, n, 8, ARG_NAME fn);
		break;
	case 4:
		while (!bsort_complete(ptr, n, 4, ARG_NAME fn))
			bsort_xform(ptr, max, n, 4, ARG_NAME fn);
		break;
	case 2:
		while (!bsort_complete(ptr, n, 2, ARG_NAME fn))
			bsort_xform(ptr, max, n, 2, ARG_NAME fn);
		break;
	case 1:
		while (!bsort_complete(ptr, n, 1, ARG_NAME fn))
			bsort_xform(ptr, max, n, 1, ARG_NAME fn);
		break;
	case 0:
		/* undefined behaviour */
		break;
	default:
		while (!bsort_complete(ptr, n, es, ARG_NAME fn))
			bsort_xform(ptr, max, n, es, ARG_NAME fn);
		break;
	}
}

#if defined(I_AM_BSORT_R)
void
bsort_r(void *a, size_t n, size_t es, cmp_t *cmp, void *arg)
{
	local_bsort(a, n, es, cmp, arg);
}
#elif defined(I_AM_BSORT_S)
errno_t
bsort_s(void *a, rsize_t n, rsize_t es, cmp_t *cmp, void *arg)
{
	if (n > RSIZE_MAX) {
		__throw_constraint_handler_s("bsort_s : n > RSIZE_MAX", EINVAL);
		return (EINVAL);
	} else if (es > RSIZE_MAX) {
		__throw_constraint_handler_s("bsort_s : es > RSIZE_MAX",
		    EINVAL);
		return (EINVAL);
	} else if (n != 0) {
		if (a == NULL) {
			__throw_constraint_handler_s("bsort_s : a == NULL",
			    EINVAL);
			return (EINVAL);
		} else if (cmp == NULL) {
			__throw_constraint_handler_s("bsort_s : cmp == NULL",
			    EINVAL);
			return (EINVAL);
		} else if (es <= 0) {
			__throw_constraint_handler_s("bsort_s : es <= 0",
			    EINVAL);
			return (EINVAL);
		}
	}

	local_bsort(a, n, es, cmp, arg);
	return (0);
}
#else
void
bsort(void *a, size_t n, size_t es, cmp_t *cmp)
{
	local_bsort(a, n, es, cmp);
}
#endif

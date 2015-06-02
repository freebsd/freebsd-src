/*-
 * Copyright (c) 2015 Munraj Vadera
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <err.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cheritest.h"

#define	NPAGES		1000
#define	NPATTERN	16
#define	SLEEPTIME	5

static uint64_t orig_pattern[NPATTERN + 1] = {
	0xFFEEDDCCBBAA9988,
	0x7766554433221100,
	0x1010101010101010,
	0x2020202020202020,
	0x3030303030303030,
	0x4040404040404040,
	0x5050505050505050,
	0x6060606060606060,
	0x7070707070707070,
	0x8080808080808080,
	0x9090909090909090,
	0xA0A0A0A0A0A0A0A0,
	0xB0B0B0B0B0B0B0B0,
	0xC0C0C0C0C0C0C0C0,
	0xD0D0D0D0D0D0D0D0,
	0xE0E0E0E0E0E0E0E0,
	0x0000000000000000
};

static uint64_t pattern[NPATTERN + 1];

#define	PRINTF(fmt, ...)	do {} while (0)
#define	CNE(p1, p2)	cne2(p1, p2)
#define	caps(p)		caps2(#p, (p))
static const char	*caps2(const char *nam, __capability void *p);
static int		 cne2(__capability void *p1, __capability void *p2);
static int		 dotest(int force_pageout);
static void		 init_patterns(void);
static void		 mix_patterns(void);
static uint64_t		 quickhash(uint64_t v);

void
cheritest_vm_swap(const struct cheri_test *ctp __unused)
{

	(void)dotest(1);
}

static const char *
caps2(const char *nam, __capability void *p)
{
	static char s[512];

	snprintf(s, sizeof(s),
	    "%s: b=%zx l=%zx o=%zx t=%d s=%d ty=%zx p=%zx",
	    nam,
	    (size_t)cheri_getbase(p),
	    (size_t)cheri_getlen(p),
	    (size_t)cheri_getoffset(p),
	    (int)cheri_gettag(p),
	    (int)cheri_getsealed(p),
	    (size_t)cheri_gettype(p),
	    (size_t)cheri_getperm(p));

	return (s);
}

static int
cne2(__capability void *p1, __capability void *p2)
{

	if (cheri_gettag(p1) != cheri_gettag(p2) ||
	    cheri_getbase(p1) != cheri_getbase(p2) ||
	    cheri_getlen(p1) != cheri_getlen(p2) ||
	    cheri_getoffset(p1) != cheri_getoffset(p2) ||
	    cheri_getsealed(p1) != cheri_getsealed(p2) ||
	    cheri_gettype(p1) != cheri_gettype(p2) ||
	    cheri_getperm(p1) != cheri_getperm(p2))
		return (1);

	return (0);
}

static void
init_patterns(void)
{

	memcpy(pattern, orig_pattern, (NPATTERN + 1) * sizeof(uint64_t));
}

static void
mix_patterns(void)
{
	uint64_t a0, a1, a2, a3, h, t, p[128];
	int i, j, k;

	for (i = 0; i < 16; i++) {
		p[i] = pattern[i];
	}
	for (i = 16; i < 128; i++) {
		p[i] = (p[i - 1] << 1) ^
		    (p[i - 5] >> 2) ^ p[i - 8] ^ p[i - 16];
	}

	a0 = 0x02468ACF13579BDE;
	a1 = 0x1032547698BADCFE;
	a2 = 0x0F1E2D3C4B5A6978;
	a3 = 0xF1E1D1C1A2B2C2D2;

	for (j = 0; j < 16; j++) {
		h = 0;
		for (i = 0; i < 128; i++) {
			k = i % 4;
			switch (k) {
			case 0:
				t = (a0 | a1) ^ (~a2 & a3);
				break;
			case 1:
				t = a0 ^ ~a1 ^ a2 ^ ~a3;
				break;
			case 2:
				t = (a0 << 1) ^ (a1 << 1) ^
				    (a2 >> 1) ^ (a3 >> 1);
				break;
			case 3:
				t = (a0 ^ ~a1) & (a2 | a3);
				break;
			}
			a3 = a2 ^ p[i];
			a2 = a1 >> 1;
			a1 = a0 << 1;
			a0 = t;

			h += a0 + a1 + a2 + a3;
		}
		p[j] = h;
		pattern[j] = h;
	}
}

static uint64_t
quickhash(uint64_t v)
{
	uint64_t a0, a1, a2, a3, t, h;
	int i, j;

	a0 = v;
	a1 = 0x02468ACF13579BDE;
	a2 = 0x1032547698BADCFE;
	a3 = 0x0F1E2D3C4B5A6978;

	h = 0;
	for (i = 0; i < 128; i++) {
		j = i % 4;
		switch (j) {
		case 0:
			t = (a0 | a1) ^ (~a2 & a3);
			break;
		case 1:
			t = a0 ^ ~a1 ^ a2 ^ ~a3;
			break;
		case 2:
			t = (a0 << 1) ^ (a1 << 1) ^
			    (a2 >> 1) ^ (a3 >> 1);
			break;
		case 3:
			t = (a0 ^ ~a1) & (a2 | a3);
			break;
		}
		a3 = a2;
		a2 = a1 >> 1;
		a1 = a0 << 1;
		a0 = t;

		h += a0 + a1 + a2 + a3;
	}

	return (h);
}

static int
dotest(int force_pageout)
{
	__capability void **p;
	__capability void *tmp, *sealer;
	uint64_t hash, tags, found_tags;
	size_t i, j, k, nsealed, ntagged, pagesz, sz;
	int mismatches, rc, want_tag, want_seal;
	char mincore_values[NPAGES];

	pagesz = getpagesize();
	sz = pagesz * NPAGES;
	p = mmap(NULL, sz, PROT_READ | PROT_WRITE,
	    MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (p == (void *)MAP_FAILED)
		err(1, "mmap");
	PRINTF("p=%p\n", p);

#define	LOOP_INNER do {							\
		if (j == 8 * sizeof(*pattern)) {			\
			j = 0;						\
			k++;						\
			tags = pattern[k];				\
			if (tags == 0) {				\
				mix_patterns();				\
				k = 0;					\
				tags = pattern[k];			\
			}						\
			hash = quickhash(pattern[k]);			\
		}							\
									\
		want_tag = tags & 1;					\
		want_seal = hash & (1 << j);				\
									\
		tmp = cheri_ptr((void *)(pattern[k] & 0xFFFFFFFF), i);	\
		if (!want_tag) {					\
			*(uint64_t *)&tmp = 0xDEADFEEE0000DEAD;		\
		} else							\
			ntagged++;					\
									\
		/* "Randomly" seal. */					\
		if (want_seal && want_tag) {				\
			sealer = cheri_ptr(				\
			    (void*)(((hash & ~0xFFULL) | j) & 0x007FFFFF),\
			    ((j << 8) | i | 0x000F0000) & 0x00FFFFFF);	\
			sealer = cheri_setoffset(sealer, i);		\
			tmp = cheri_seal(tmp, sealer);			\
			nsealed++;					\
		}							\
									\
		j++;							\
		tags >>= 1;						\
} while (0)

	/* Set tags based on patterns. */
	PRINTF("Filling pages...\n");
	init_patterns();
	j = 8 * sizeof(*pattern);
	k = NPATTERN;
	nsealed = 0;
	ntagged = 0;
	for (i = 0; i < sz / sizeof(__capability void *); i++) {
		LOOP_INNER;
		p[i] = tmp;
	}
	PRINTF("%zu sealed and %zu tagged out of %zu\n",
	    nsealed, ntagged, i);

	if (force_pageout) {
		PRINTF("Paging out...\n");
		rc = msync(p, sz, MS_PAGEOUT);
		if (rc == -1)
			cheritest_failure_errx("msync(MS_PAGEOUT) failed\n");
		rc = mincore(p, sz, mincore_values);
		if (rc < 0)
			cheritest_failure_errx("mincore() failed\n");
		else {
			for (i = 0; i < NPAGES; i++) {
				if (mincore_values[i] & MINCORE_INCORE) {
					cheritest_failure_errx(
					    "mincore() reports page %u is "
					    "in core\n",
					    (unsigned int)i);
				}
			}
		}
	}

	/* Allow inspection via job backgrounding. */
	/*PRINTF("Sleeping for %d sec...\n", SLEEPTIME);
	sleep(SLEEPTIME);*/

	/* Scan for validity. */
	PRINTF("Checking pages...\n");
	init_patterns();
	mismatches = 0;
	j = 8 * sizeof(*pattern);
	k = NPATTERN;
	found_tags = 0;
	nsealed = 0;
	ntagged = 0;
	for (i = 0; i < sz / sizeof(__capability void *); i++) {
		if (j == 8 * sizeof(*pattern)) {
			if (found_tags != pattern[k])
				warnx("found: 0x%llx expected: 0x%llx\n",
				    (unsigned long long)found_tags,
				    (unsigned long long)pattern[k]);
			found_tags = 0;
		}

		LOOP_INNER;

		if (cheri_gettag(p[i]))
			found_tags |= (1ULL << (uint64_t)(j - 1));

		if (CNE(p[i], tmp)) {
			warnx("mismatch at %zu:\n", i);
			warnx("%s\n", caps(tmp));
			warnx("%s\n", caps(p[i]));
			mismatches++;
		}
	}

	if (mismatches == 0)
		cheritest_success();
	else
		cheritest_failure_errx("%d mismatches\n", mismatches);

	return (0);
}

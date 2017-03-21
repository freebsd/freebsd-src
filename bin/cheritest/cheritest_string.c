/*-
 * Copyright (c) 2012-2014 David T. Chisnall
 * Copyright (c) 2015 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
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

#include <sys/cdefs.h>

#if !__has_feature(capabilities)
#error "This code requires a CHERI-aware compiler"
#endif

#include <sys/types.h>

#include <cheri/cheri.h>
#include <cheri/cheric.h>

#include <string.h>

#include "cheritest.h"

#define	CAP(x)	((__capability void*)(x))

/*
 * Test structure which will be memcpy'd.  Contains data and a capability in
 * the middle.  The capability must be aligned, but memcpy should work for any
 * partial copy of this structure that includes the capability, as long as both
 * have the correct alignment.
 */
struct Test
{
	char pad0[32];
	__capability void *y;
	char pad1[32];
};

/*
 * Check that the copy has the data that we expect it to contain.  The start
 * and end parameters describe the range in the padding to check.  For partial
 * copies, the uncopied range will contain nonsense.
 */
static void
check(struct Test *t1, int start, int end)
{
	int i;

	for (i = start; i < 32; i++)
		if (t1->pad0[i] != i)
			cheritest_failure_errx("t1->pad0[%d] != %d", i, i);
	if ((void*)t1->y != t1)
		cheritest_failure_errx("t1->y != t1");
	if (!cheri_gettag(t1->y))
		cheritest_failure_errx("t1->y is untagged");
	for (i = 0 ; i < end ; i++)
		if (t1->pad1[i] != i)
			cheritest_failure_errx("t1->pad1[%d] != %d", i, i);
}

/*
 * Write an obviously invalid byte pattern over the output structure.
 */
static void
invalidate(struct Test *t1)
{
	size_t i;
	unsigned char *x = (unsigned char*)t1;

	for (i = 0; i < sizeof(*t1); i++)
		*x = 0xa5;
}

void
test_string_memcpy_c(const struct cheri_test *ctp __unused)
{
	int i;
	__capability void *cpy;
	struct Test t1, t2;

	invalidate(&t1);
	for (i = 0; i < 32; i++) {
		t1.pad0[i] = i;
		t1.pad1[i] = i;
	}
	t1.y = CAP(&t2);

	/* Simple case: aligned start and end */
	invalidate(&t2);
	cpy = memcpy_c(CAP(&t2), CAP(&t1), sizeof(t1));
	if ((void *)cpy != &t2)
		cheritest_failure_errx("memcpy_c did not return dst (&t2)");
	check(&t2, 0, 32);

	/* Test that it still works with an unaligned start... */
	invalidate(&t2);
	cpy = memcpy_c(CAP(&t2.pad0[3]), CAP(&t1.pad0[3]), sizeof(t1) - 3);
	if ((void*)cpy != &t2.pad0[3])
		cheritest_failure_errx("memcpy_c did not return dst "
		    "(&t2.pad0[3])");
	check(&t2, 3, 32);

	/* ...or and unaligned end... */
	invalidate(&t2);
	cpy = memcpy_c(CAP(&t2), CAP(&t1), sizeof(t1) - 3);
	if ((void *)cpy != &t2)
		cheritest_failure_errx("memcpy_c did not return dst (&t2)");
	check(&t2, 0, 29);

	/* ...or both... */
	invalidate(&t2);
	cpy = memcpy_c(CAP(&t2.pad0[3]), CAP(&t1.pad0[3]), sizeof(t1) - 6);
	if ((void*)cpy != &t2.pad0[3])
		cheritest_failure_errx("memcpy_c did not return dst "
		    "(&t2.pad0[3])");
	check(&t2, 3, 29);

	/* ...and case where the alignment is different for both... */
	invalidate(&t2);
	cpy = memcpy_c(CAP(&t2), CAP(&t1.pad0[1]), sizeof(t1) - 1);
	if ((void*)cpy != &t2)
		cheritest_failure_errx("memcpy_c did not return dst (&t2)");
	/* This should have invalidated the capability */
	if (cheri_gettag(t2.y) != 0)
		cheritest_failure_errx("dst has capability after unaligned "
		    "write");
	for (i = 0; i < 31; i++) {
		if (t2.pad0[i] != i+1)
			cheritest_failure_errx("t2.pad0[%d] != %d", i, i+1);
		if (t2.pad1[i] != i+1)
			cheritest_failure_errx("t2.pad1[%d] != %d", i, i+1);
	}

	/*
	 * ...and finally finally tests that offsets are taken into
	 * account when checking alignment.  These are regression tests
	 * for a bug in memcpy_c.
	 */
	/* aligned base, unaligned offset + base */
	invalidate(&t2);
	cpy = memcpy_c(
	    __builtin_mips_cheri_cap_offset_increment(CAP(&t2), 3),
	    __builtin_mips_cheri_cap_offset_increment(CAP(&t1), 3),
	    sizeof(t1)-6);
	if ((void*)cpy != &t2.pad0[3])
		cheritest_failure_errx("memcpy_c did not return dst "
		    "(&t2.pad0[3])");
	check(&t2, 3, 29);

	/* unaligned base, aligned offset + base */
	// FIXME: This currently gives an aligned base.  We should make the CAP
	// macro take a base and length so that it can do CIncBase / CSetLen on
	// CHERI256, CFromPtr / CSetBounds on CHERI128
	invalidate(&t2);
	cpy = memcpy_c(
	    __builtin_mips_cheri_cap_offset_increment(CAP(t2.pad0-1), 1),
	    __builtin_mips_cheri_cap_offset_increment(CAP(t1.pad0-1), 1),
	    sizeof(t1));
	if ((void*)cpy != &t2.pad0)
		cheritest_failure_errx("(void*)cpy != &t2.pad0");
	check(&t2, 0, 32);

	/* Unaligned, but offset=32 */
	invalidate(&t2);
	cpy = memmove_c(
	    __builtin_mips_cheri_cap_offset_increment(CAP(t2.pad0-1), 32),
	    __builtin_mips_cheri_cap_offset_increment(CAP(t1.pad0-1), 32),
	    sizeof(t1) - 31);
	if ((void*)cpy != t2.pad0+31)
		cheritest_failure_errx("(void*)cpy != t2.pad0+31");
	check(&t2, 31, 32);

	cheritest_success();
}

void
test_string_memcpy(const struct cheri_test *ctp __unused)
{
	int i;
	void *copy;
	struct Test t1, t2;

	invalidate(&t1);
	for (i = 0; i < 32; i++) {
		t1.pad0[i] = i;
		t1.pad1[i] = i;
	}
	t1.y = CAP(&t2);

	/* Simple case: aligned start and end */
	invalidate(&t2);
	copy = memcpy(&t2, &t1, sizeof(t1));
	if (copy != &t2)
		cheritest_failure_errx("copy != &t2");
	check(&t2, 0, 32);

	/* Test that it still works with an unaligned start... */
	invalidate(&t2);
	copy = memcpy(&t2.pad0[3], &t1.pad0[3], sizeof(t1) - 3);
	if ((void*)copy != &t2.pad0[3])
		cheritest_failure_errx("memcpy_c did not return dst "
		    "(&t2.pad0[3])");
	check(&t2, 3, 32);

	/* ...or an unaligned end... */
	invalidate(&t2);
	copy = memcpy(&t2, &t1, sizeof(t1) - 3);
	if (copy != &t2)
		cheritest_failure_errx("copy != &t2");
	check(&t2, 0, 29);

	/* ...or both... */
	invalidate(&t2);
	copy = memcpy(&t2.pad0[3], &t1.pad0[3], sizeof(t1) - 6);
	if ((void*)copy != &t2.pad0[3])
		cheritest_failure_errx("memcpy_c did not return dst "
		    "(&t2.pad0[3])");
	check(&t2, 3, 29);

	/* ...and finally a case where the alignment is different for both */
	copy = memcpy(&t2, &t1.pad0[1], sizeof(t1) - 1);
	if (copy != &t2)
		cheritest_failure_errx("copy != &t2");
	if (cheri_gettag(t2.y) != 0)
		cheritest_failure_errx("dst has capability after unaligned "
		    "write");
	for (i = 0; i < 31; i++) {
		if (t2.pad0[i] != i+1)
			cheritest_failure_errx("t2.pad0[%d] != %d", i, i+1);
		if (t2.pad1[i] != i+1)
			cheritest_failure_errx("t2.pad1[%d] != %d", i, i+1);
	}

	cheritest_success();
}

void
test_string_memmove_c(const struct cheri_test *ctp __unused)
{
	int i;
	__capability void *cpy;
	struct Test t1, t2;

	invalidate(&t1);
	for (i = 0; i < 32; i++) {
		t1.pad0[i] = i;
		t1.pad1[i] = i;
	}
	t1.y = CAP(&t2);

	/* Simple case: aligned start and end */
	invalidate(&t2);
	cpy = memmove_c(CAP(&t2), CAP(&t1), sizeof(t1));
	if ((void *)cpy != &t2)
		cheritest_failure_errx("memmove_c did not return dst (&t2)");
	check(&t2, 0, 32);

	/* Test that it still works with an unaligned start... */
	invalidate(&t2);
	cpy = memmove_c(CAP(&t2.pad0[3]), CAP(&t1.pad0[3]), sizeof(t1) - 3);
	if ((void*)cpy != &t2.pad0[3])
		cheritest_failure_errx("memmove_c did not return dst "
		    "(&t2.pad0[3])");
	check(&t2, 3, 32);

	/* ...or and unaligned end... */
	invalidate(&t2);
	cpy = memmove_c(CAP(&t2), CAP(&t1), sizeof(t1) - 3);
	if ((void *)cpy != &t2)
		cheritest_failure_errx("memmove_c did not return dst (&t2)");
	check(&t2, 0, 29);

	/* ...or both... */
	invalidate(&t2);
	cpy = memmove_c(CAP(&t2.pad0[3]), CAP(&t1.pad0[3]), sizeof(t1) - 6);
	if ((void*)cpy != &t2.pad0[3])
		cheritest_failure_errx("memmove_c did not return dst "
		    "(&t2.pad0[3])");
	check(&t2, 3, 29);

	/* ...and case where the alignment is different for both... */
	invalidate(&t2);
	cpy = memmove_c(CAP(&t2), CAP(&t1.pad0[1]), sizeof(t1) - 1);
	if ((void*)cpy != &t2)
		cheritest_failure_errx("memmove_c did not return dst (&t2)");
	/* This should have invalidated the capability */
	if (cheri_gettag(t2.y) != 0)
		cheritest_failure_errx("dst has capability after unaligned "
		    "write");
	for (i = 0; i < 31; i++) {
		if (t2.pad0[i] != i+1)
			cheritest_failure_errx("t2.pad0[%d] != %d", i, i+1);
		if (t2.pad1[i] != i+1)
			cheritest_failure_errx("t2.pad1[%d] != %d", i, i+1);
	}

	/*
	 * ...and finally finally tests that offsets are taken into
	 * account when checking alignment.  These are regression tests
	 * for a bug in memmove_c.
	 */
	/* aligned base, unaligned offset + base */
	invalidate(&t2);
	cpy = memmove_c(
	    __builtin_mips_cheri_cap_offset_increment(CAP(&t2), 3),
	    __builtin_mips_cheri_cap_offset_increment(CAP(&t1), 3),
	    sizeof(t1)-6);
	if ((void*)cpy != &t2.pad0[3])
		cheritest_failure_errx("memmove_c did not return dst "
		    "(&t2.pad0[3])");
	check(&t2, 3, 29);

	/* unaligned base, aligned offset + base */
	invalidate(&t2);
	cpy = memmove_c(
	    __builtin_mips_cheri_cap_offset_increment(CAP(t2.pad0-1), 1),
	    __builtin_mips_cheri_cap_offset_increment(CAP(t1.pad0-1), 1),
	    sizeof(t1));
	if ((void*)cpy != &t2.pad0)
		cheritest_failure_errx("(void*)cpy != &t2.pad0");
	check(&t2, 0, 32);

	/* Unaligned, but offset=32 */
	invalidate(&t2);
	cpy = memmove_c(
	    __builtin_mips_cheri_cap_offset_increment(CAP(t2.pad0-1), 32),
	    __builtin_mips_cheri_cap_offset_increment(CAP(t1.pad0-1), 32),
	    sizeof(t1) - 31);
	if ((void*)cpy != t2.pad0+31)
		cheritest_failure_errx("(void*)cpy != t2.pad0+31");
	check(&t2, 31, 32);

	/* XXX-BD: test overlapping cases */

	cheritest_success();
}

void
test_string_memmove(const struct cheri_test *ctp __unused)
{
	int i;
	void *copy;
	struct Test t1, t2;

	invalidate(&t1);
	for (i = 0; i < 32; i++) {
		t1.pad0[i] = i;
		t1.pad1[i] = i;
	}
	t1.y = CAP(&t2);

	/* Simple case: aligned start and end */
	invalidate(&t2);
	copy = memmove(&t2, &t1, sizeof(t1));
	if (copy != &t2)
		cheritest_failure_errx("copy != &t2");
	check(&t2, 0, 32);

	/* Test that it still works with an unaligned start... */
	invalidate(&t2);
	copy = memmove(&t2.pad0[3], &t1.pad0[3], sizeof(t1) - 3);
	if ((void*)copy != &t2.pad0[3])
		cheritest_failure_errx("memmove_c did not return dst "
		    "(&t2.pad0[3])");
	check(&t2, 3, 32);

	/* ...or an unaligned end... */
	invalidate(&t2);
	copy = memmove(&t2, &t1, sizeof(t1) - 3);
	if (copy != &t2)
		cheritest_failure_errx("copy != &t2");
	check(&t2, 0, 29);

	/* ...or both... */
	invalidate(&t2);
	copy = memmove(&t2.pad0[3], &t1.pad0[3], sizeof(t1) - 6);
	if ((void*)copy != &t2.pad0[3])
		cheritest_failure_errx("memmove_c did not return dst "
		    "(&t2.pad0[3])");
	check(&t2, 3, 29);

	/* ...and finally a case where the alignment is different for both */
	copy = memmove(&t2, &t1.pad0[1], sizeof(t1) - 1);
	if (copy != &t2)
		cheritest_failure_errx("copy != &t2");
	if (cheri_gettag(t2.y) != 0)
		cheritest_failure_errx("dst has capability after unaligned "
		    "write");
	for (i = 0; i < 31; i++) {
		if (t2.pad0[i] != i+1)
			cheritest_failure_errx("t2.pad0[%d] != %d", i, i+1);
		if (t2.pad1[i] != i+1)
			cheritest_failure_errx("t2.pad1[%d] != %d", i, i+1);
	}

	/* XXX-BD: test overlapping cases */

	cheritest_success();
}

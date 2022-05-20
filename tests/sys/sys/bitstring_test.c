/*-
 * Copyright (c) 2014 Spectra Logic Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * $FreeBSD$
 */

#include <sys/param.h>

#include <bitstring.h>
#include <stdio.h>

#include <atf-c.h>

typedef void (testfunc_t)(bitstr_t *bstr, int nbits, const char *memloc);

static void
bitstring_run_stack_test(testfunc_t *test, int nbits)
{
	bitstr_t bit_decl(bitstr, nbits);

	test(bitstr, nbits, "stack");
}

static void
bitstring_run_heap_test(testfunc_t *test, int nbits)
{
	bitstr_t *bitstr = bit_alloc(nbits);

	test(bitstr, nbits, "heap");
}

static void
bitstring_test_runner(testfunc_t *test)
{
	const int bitstr_sizes[] = {
		0,
		1,
		_BITSTR_BITS - 1,
		_BITSTR_BITS,
		_BITSTR_BITS + 1,
		2 * _BITSTR_BITS - 1,
		2 * _BITSTR_BITS,
		1023,
		1024
	};

	for (unsigned long i = 0; i < nitems(bitstr_sizes); i++) {
		bitstring_run_stack_test(test, bitstr_sizes[i]);
		bitstring_run_heap_test(test, bitstr_sizes[i]);
	}
}

#define	BITSTRING_TC_DEFINE(name)				\
ATF_TC_WITHOUT_HEAD(name);					\
static testfunc_t name ## _test;				\
								\
ATF_TC_BODY(name, tc)						\
{								\
	bitstring_test_runner(name ## _test);			\
}								\
								\
static void							\
name ## _test(bitstr_t *bitstr, int nbits, const char *memloc)

#define	BITSTRING_TC_ADD(tp, name)				\
do {								\
	ATF_TP_ADD_TC(tp, name);				\
} while (0)

ATF_TC_WITHOUT_HEAD(bitstr_in_struct);
ATF_TC_BODY(bitstr_in_struct, tc)
{
	struct bitstr_containing_struct {
		bitstr_t bit_decl(bitstr, 8);
	} test_struct;

	bit_nclear(test_struct.bitstr, 0, 8);
}

ATF_TC_WITHOUT_HEAD(bitstr_size);
ATF_TC_BODY(bitstr_size, tc)
{
	size_t sob = sizeof(bitstr_t);

	ATF_CHECK_EQ(0, bitstr_size(0));
	ATF_CHECK_EQ(sob, bitstr_size(1));
	ATF_CHECK_EQ(sob, bitstr_size(sob * 8));
	ATF_CHECK_EQ(2 * sob, bitstr_size(sob * 8 + 1));
}

BITSTRING_TC_DEFINE(bit_set)
/* bitstr_t *bitstr, int nbits, const char *memloc */
{
	memset(bitstr, 0, bitstr_size(nbits));
	
	for (int i = 0; i < nbits; i++) {
		bit_set(bitstr, i);

		for (int j = 0; j < nbits; j++) {
			ATF_REQUIRE_MSG(bit_test(bitstr, j) == (j == i) ? 1 : 0,
			    "bit_set_%d_%s: Failed on bit %d",
			    nbits, memloc, i);
		}

		bit_clear(bitstr, i);
	}
}

BITSTRING_TC_DEFINE(bit_clear)
/* bitstr_t *bitstr, int nbits, const char *memloc */
{
	int i, j;

	memset(bitstr, 0xFF, bitstr_size(nbits));
	for (i = 0; i < nbits; i++) {
		bit_clear(bitstr, i);

		for (j = 0; j < nbits; j++) {
			ATF_REQUIRE_MSG(bit_test(bitstr, j) == (j == i) ? 0 : 1,
			    "bit_clear_%d_%s: Failed on bit %d",
			    nbits, memloc, i);
		}

		bit_set(bitstr, i);
	}
}

BITSTRING_TC_DEFINE(bit_ffs)
/* bitstr_t *bitstr, int nbits, const char *memloc */
{
	int i;
	int found_set_bit;

	memset(bitstr, 0, bitstr_size(nbits));
	bit_ffs(bitstr, nbits, &found_set_bit);
	ATF_REQUIRE_MSG(found_set_bit == -1,
	    "bit_ffs_%d_%s: Failed all clear bits.", nbits, memloc);

	for (i = 0; i < nbits; i++) {
		memset(bitstr, 0xFF, bitstr_size(nbits));
		if (i > 0)
			bit_nclear(bitstr, 0, i - 1);

		bit_ffs(bitstr, nbits, &found_set_bit);
		ATF_REQUIRE_MSG(found_set_bit == i,
		    "bit_ffs_%d_%s: Failed on bit %d, Result %d",
		    nbits, memloc, i, found_set_bit);
	}
}

BITSTRING_TC_DEFINE(bit_ffc)
/* bitstr_t *bitstr, int nbits, const char *memloc */
{
	int i;
	int found_clear_bit;

	memset(bitstr, 0xFF, bitstr_size(nbits));
	bit_ffc(bitstr, nbits, &found_clear_bit);
	ATF_REQUIRE_MSG(found_clear_bit == -1,
	    "bit_ffc_%d_%s: Failed all set bits.", nbits, memloc);

	for (i = 0; i < nbits; i++) {
		memset(bitstr, 0, bitstr_size(nbits));
		if (i > 0)
			bit_nset(bitstr, 0, i - 1);

		bit_ffc(bitstr, nbits, &found_clear_bit);
		ATF_REQUIRE_MSG(found_clear_bit == i,
		    "bit_ffc_%d_%s: Failed on bit %d, Result %d",
		    nbits, memloc, i, found_clear_bit);
	}
}

BITSTRING_TC_DEFINE(bit_ffs_at)
/* bitstr_t *bitstr, int nbits, const char *memloc */
{
	int i;
	int found_set_bit;

	memset(bitstr, 0xFF, bitstr_size(nbits));
	for (i = 0; i < nbits; i++) {
		bit_ffs_at(bitstr, i, nbits, &found_set_bit);
		ATF_REQUIRE_MSG(found_set_bit == i,
		    "bit_ffs_at_%d_%s: Failed on bit %d, Result %d",
		    nbits, memloc, i, found_set_bit);
	}

	memset(bitstr, 0, bitstr_size(nbits));
	for (i = 0; i < nbits; i++) {
		bit_ffs_at(bitstr, i, nbits, &found_set_bit);
		ATF_REQUIRE_MSG(found_set_bit == -1,
		    "bit_ffs_at_%d_%s: Failed on bit %d, Result %d",
		    nbits, memloc, i, found_set_bit);
	}

	memset(bitstr, 0x55, bitstr_size(nbits));
	for (i = 0; i < nbits; i++) {
		bit_ffs_at(bitstr, i, nbits, &found_set_bit);
		if (i == nbits - 1 && (nbits & 1) == 0) {
			ATF_REQUIRE_MSG(found_set_bit == -1,
			    "bit_ffs_at_%d_%s: Failed on bit %d, Result %d",
			    nbits, memloc, i, found_set_bit);
		} else {
			ATF_REQUIRE_MSG(found_set_bit == i + (i & 1),
			    "bit_ffs_at_%d_%s: Failed on bit %d, Result %d",
			    nbits, memloc, i, found_set_bit);
		}
	}

	memset(bitstr, 0xAA, bitstr_size(nbits));
	for (i = 0; i < nbits; i++) {
		bit_ffs_at(bitstr, i, nbits, &found_set_bit);
		if (i == nbits - 1 && (nbits & 1) != 0) {
			ATF_REQUIRE_MSG(found_set_bit == -1,
			    "bit_ffs_at_%d_%s: Failed on bit %d, Result %d",
			    nbits, memloc, i, found_set_bit);
		} else {
			ATF_REQUIRE_MSG(
			    found_set_bit == i + ((i & 1) ? 0 : 1),
			    "bit_ffs_at_%d_%s: Failed on bit %d, Result %d",
			    nbits, memloc, i, found_set_bit);
		}
	}

	/* Pass a start value beyond the size of the bit string */
	bit_ffs_at(bitstr, nbits, nbits, &found_set_bit);
	ATF_REQUIRE_MSG(found_set_bit == -1,
			"bit_ffs_at_%d_%s: Failed with high start value of %d, Result %d",
			nbits, memloc, nbits, found_set_bit);

	bit_ffs_at(bitstr, nbits + 3, nbits, &found_set_bit);
	ATF_REQUIRE_MSG(found_set_bit == -1,
			"bit_ffs_at_%d_%s: Failed with high start value of %d, Result %d",
			nbits, memloc, nbits + 3, found_set_bit);
}

BITSTRING_TC_DEFINE(bit_ffc_at)
/* bitstr_t *bitstr, int nbits, const char *memloc */
{
	int i, found_clear_bit;

	memset(bitstr, 0, bitstr_size(nbits));
	for (i = 0; i < nbits; i++) {
		bit_ffc_at(bitstr, i, nbits, &found_clear_bit);
		ATF_REQUIRE_MSG(found_clear_bit == i,
		    "bit_ffc_at_%d_%s: Failed on bit %d, Result %d",
		    nbits, memloc, i, found_clear_bit);
	}

	memset(bitstr, 0xFF, bitstr_size(nbits));
	for (i = 0; i < nbits; i++) {
		bit_ffc_at(bitstr, i, nbits, &found_clear_bit);
		ATF_REQUIRE_MSG(found_clear_bit == -1,
		    "bit_ffc_at_%d_%s: Failed on bit %d, Result %d",
		    nbits, memloc, i, found_clear_bit);
	}

	memset(bitstr, 0x55, bitstr_size(nbits));
	for (i = 0; i < nbits; i++) {
		bit_ffc_at(bitstr, i, nbits, &found_clear_bit);
		if (i == nbits - 1 && (nbits & 1) != 0) {
			ATF_REQUIRE_MSG(found_clear_bit == -1,
			    "bit_ffc_at_%d_%s: Failed on bit %d, Result %d",
			    nbits, memloc, i, found_clear_bit);
		} else {
			ATF_REQUIRE_MSG(
			    found_clear_bit == i + ((i & 1) ? 0 : 1),
			    "bit_ffc_at_%d_%s: Failed on bit %d, Result %d",
			    nbits, memloc, i, found_clear_bit);
		}
	}

	memset(bitstr, 0xAA, bitstr_size(nbits));
	for (i = 0; i < nbits; i++) {
		bit_ffc_at(bitstr, i, nbits, &found_clear_bit);
		if (i == nbits - 1 && (nbits & 1) == 0) {
			ATF_REQUIRE_MSG(found_clear_bit == -1,
			    "bit_ffc_at_%d_%s: Failed on bit %d, Result %d",
			    nbits, memloc, i, found_clear_bit);
		} else {
			ATF_REQUIRE_MSG(found_clear_bit == i + (i & 1),
			    "bit_ffc_at_%d_%s: Failed on bit %d, Result %d",
			    nbits, memloc, i, found_clear_bit);
		}
	}

	/* Pass a start value beyond the size of the bit string */
	bit_ffc_at(bitstr, nbits, nbits, &found_clear_bit);
	ATF_REQUIRE_MSG(found_clear_bit == -1,
			"bit_ffc_at_%d_%s: Failed with high start value, Result %d",
			nbits, memloc, found_clear_bit);

	bit_ffc_at(bitstr, nbits + 3, nbits, &found_clear_bit);
	ATF_REQUIRE_MSG(found_clear_bit == -1,
			"bit_ffc_at_%d_%s: Failed with high start value of %d, Result %d",
			nbits, memloc, nbits + 3, found_clear_bit);
}

BITSTRING_TC_DEFINE(bit_ffc_area_at_all_or_nothing)
/* bitstr_t *bitstr, int nbits, const char *memloc */
{
	int found;

	memset(bitstr, 0, bitstr_size(nbits));
	if (nbits % _BITSTR_BITS != 0)
		bit_nset(bitstr, nbits, roundup2(nbits, _BITSTR_BITS) - 1);

	for (int start = 0; start < nbits; start++) {
		for (int size = 1; size < nbits - start; size++) {
			bit_ffc_area_at(bitstr, start, nbits, size, &found);
			ATF_REQUIRE_EQ_MSG(start, found,
			    "bit_ffc_area_at_%d_%s: "
			    "Did not find %d clear bits at %d",
			    nbits, memloc, size, start);
		}
	}

	memset(bitstr, 0xff, bitstr_size(nbits));
	if (nbits % _BITSTR_BITS != 0)
		bit_nclear(bitstr, nbits, roundup2(nbits, _BITSTR_BITS) - 1);

	for (int start = 0; start < nbits; start++) {
		for (int size = 1; size < nbits - start; size++) {
			bit_ffc_area_at(bitstr, start, nbits, size, &found);
			ATF_REQUIRE_EQ_MSG(-1, found,
			    "bit_ffc_area_at_%d_%s: "
			    "Found %d clear bits at %d",
			    nbits, memloc, size, start);
		}
	}
}

BITSTRING_TC_DEFINE(bit_ffs_area_at_all_or_nothing)
/* bitstr_t *bitstr, int nbits, const char *memloc */
{
	int found;

	memset(bitstr, 0, bitstr_size(nbits));
	if (nbits % _BITSTR_BITS != 0)
		bit_nset(bitstr, nbits, roundup2(nbits, _BITSTR_BITS) - 1);

	for (int start = 0; start < nbits; start++) {
		for (int size = 1; size < nbits - start; size++) {
			bit_ffs_area_at(bitstr, start, nbits, size, &found);
			ATF_REQUIRE_EQ_MSG(-1, found,
			    "bit_ffs_area_at_%d_%s: "
			    "Found %d set bits at %d",
			    nbits, memloc, size, start);
		}
	}

	memset(bitstr, 0xff, bitstr_size(nbits));
	if (nbits % _BITSTR_BITS != 0)
		bit_nclear(bitstr, nbits, roundup2(nbits, _BITSTR_BITS) - 1);

	for (int start = 0; start < nbits; start++) {
		for (int size = 1; size < nbits - start; size++) {
			bit_ffs_area_at(bitstr, start, nbits, size, &found);
			ATF_REQUIRE_EQ_MSG(start, found,
			    "bit_ffs_area_at_%d_%s: "
			    "Did not find %d set bits at %d",
			    nbits, memloc, size, start);
		}
	}
}

ATF_TC_WITHOUT_HEAD(bit_ffs_area);
ATF_TC_BODY(bit_ffs_area, tc)
{
	const int nbits = 72;
	bitstr_t bit_decl(bitstr, nbits);
	int location;

	memset(bitstr, 0, bitstr_size(nbits));

	bit_nset(bitstr, 5, 6);

	location = 0;
	bit_ffs_area(bitstr, nbits, 3, &location);
	ATF_REQUIRE_EQ_MSG(-1, location,
	    "bit_ffs_area: found location of size 3 when only 2 bits are set");
	ATF_REQUIRE_EQ_MSG(0, bit_ntest(bitstr, 5, 7, 1),
	    "bit_ntest: found location of size 3 when only 2 bits are set");

	bit_set(bitstr, 7);

	location = 0;
	bit_ffs_area(bitstr, nbits, 3, &location);
	ATF_REQUIRE_EQ_MSG(5, location,
	    "bit_ffs_area: failed to find location of size 3 %d", location);
	ATF_REQUIRE_EQ_MSG(1, bit_ntest(bitstr, 5, 7, 1),
	    "bit_ntest: failed to find all 3 bits set");

	bit_set(bitstr, 8);

	location = 0;
	bit_ffs_area(bitstr, nbits, 3, &location);
	ATF_REQUIRE_EQ_MSG(5, location,
			"bit_ffs_area: failed to find location of size 3");

	location = 0;
	bit_ffs_area_at(bitstr, 2, nbits, 3, &location);
	ATF_REQUIRE_EQ_MSG(5, location,
			"bit_ffs_area_at: failed to find location of size 3");

	location = 0;
	bit_ffs_area_at(bitstr, 6, nbits, 3, &location);
	ATF_REQUIRE_EQ_MSG(6, location,
			"bit_ffs_area_at: failed to find location of size 3");

	location = 0;
	bit_ffs_area_at(bitstr, 8, nbits, 3, &location);
	ATF_REQUIRE_EQ_MSG(-1, location,
			"bit_ffs_area_at: found invalid location");

	bit_nset(bitstr, 69, 71);

	location = 0;
	bit_ffs_area_at(bitstr, 8, nbits, 3, &location);
	ATF_REQUIRE_EQ_MSG(69, location,
			"bit_ffs_area_at: failed to find location of size 3");

	location = 0;
	bit_ffs_area_at(bitstr, 69, nbits, 3, &location);
	ATF_REQUIRE_EQ_MSG(69, location,
			"bit_ffs_area_at: failed to find location of size 3");

	location = 0;
	bit_ffs_area_at(bitstr, 70, nbits, 3, &location);
	ATF_REQUIRE_EQ_MSG(-1, location,
			"bit_ffs_area_at: found invalid location");

	location = 0;
	bit_ffs_area_at(bitstr, 72, nbits, 3, &location);
	ATF_REQUIRE_EQ_MSG(-1, location,
			"bit_ffs_area_at: found invalid location");

	bit_nset(bitstr, 59, 67);

	location = 0;
	bit_ffs_area(bitstr, nbits, 9, &location);
	ATF_REQUIRE_EQ_MSG(59, location,
			"bit_ffs_area: failed to find location of size 9");

	location = 0;
	bit_ffs_area(bitstr, nbits, 10, &location);
	ATF_REQUIRE_EQ_MSG(-1, location,
			"bit_ffs_area: found invalid location");
}

ATF_TC_WITHOUT_HEAD(bit_ffc_area);
ATF_TC_BODY(bit_ffc_area, tc)
{
	const int nbits = 80;
	bitstr_t bit_decl(bitstr, nbits);
	int location;

	/* set all bits */
	memset(bitstr, 0xFF, bitstr_size(nbits));

	bit_clear(bitstr, 7);
	bit_clear(bitstr, 8);

	location = 0;
	bit_ffc_area(bitstr, nbits, 3, &location);
	ATF_REQUIRE_EQ_MSG(-1, location,
			"bit_ffc_area: found location of size 3 when only 2 bits are set");

	bit_clear(bitstr, 9);

	location = 0;
	bit_ffc_area(bitstr, nbits, 3, &location);
	ATF_REQUIRE_EQ_MSG(7, location,
			"bit_ffc_area: failed to find location of size 3");

	bit_clear(bitstr, 10);

	location = 0;
	bit_ffc_area(bitstr, nbits, 3, &location);
	ATF_REQUIRE_EQ_MSG(7, location,
			"bit_ffc_area: failed to find location of size 3");

	location = 0;
	bit_ffc_area_at(bitstr, 2, nbits, 3, &location);
	ATF_REQUIRE_EQ_MSG(7, location,
			"bit_ffc_area_at: failed to find location of size 3");

	location = 0;
	bit_ffc_area_at(bitstr, 8, nbits, 3, &location);
	ATF_REQUIRE_EQ_MSG(8, location,
			"bit_ffc_area_at: failed to find location of size 3");

	location = 0;
	bit_ffc_area_at(bitstr, 9, nbits, 3, &location);
	ATF_REQUIRE_EQ_MSG(-1, location,
			"bit_ffc_area_at: found invalid bit location");

	bit_clear(bitstr, 77);
	bit_clear(bitstr, 78);
	bit_clear(bitstr, 79);

	location = 0;
	bit_ffc_area_at(bitstr, 12, nbits, 3, &location);
	ATF_REQUIRE_EQ_MSG(77, location,
			"bit_ffc_area_at: failed to find location of size 3");

	location = 0;
	bit_ffc_area_at(bitstr, 77, nbits, 3, &location);
	ATF_REQUIRE_EQ_MSG(77, location,
			"bit_ffc_area_at: failed to find location of size 3");

	location = 0;
	bit_ffc_area_at(bitstr, 78, nbits, 3, &location);
	ATF_REQUIRE_EQ_MSG(-1, location,
			"bit_ffc_area_at: found invalid location");

	location = 0;
	bit_ffc_area_at(bitstr, 85, nbits, 3, &location);
	ATF_REQUIRE_EQ_MSG(-1, location,
			"bit_ffc_area_at: found invalid location");
}

BITSTRING_TC_DEFINE(bit_nclear)
/* bitstr_t *bitstr, int nbits, const char *memloc */
{
	int i, j;
	int found_set_bit;
	int found_clear_bit;

	for (i = 0; i < nbits; i++) {
		for (j = i; j < nbits; j++) {
			memset(bitstr, 0xFF, bitstr_size(nbits));
			bit_nclear(bitstr, i, j);

			bit_ffc(bitstr, nbits, &found_clear_bit);
			ATF_REQUIRE_MSG(
			    found_clear_bit == i,
			    "bit_nclear_%d_%d_%d%s: Failed with result %d",
			    nbits, i, j, memloc, found_clear_bit);

			bit_ffs_at(bitstr, i, nbits, &found_set_bit);
			ATF_REQUIRE_MSG(
			    (j + 1 < nbits) ? found_set_bit == j + 1 : -1,
			    "bit_nset_%d_%d_%d%s: Failed with result %d",
			    nbits, i, j, memloc, found_set_bit);
		}
	}
}

BITSTRING_TC_DEFINE(bit_nset)
/* bitstr_t *bitstr, int nbits, const char *memloc */
{
	int i, j;
	int found_set_bit;
	int found_clear_bit;

	for (i = 0; i < nbits; i++) {
		for (j = i; j < nbits; j++) {
			memset(bitstr, 0, bitstr_size(nbits));
			bit_nset(bitstr, i, j);

			bit_ffs(bitstr, nbits, &found_set_bit);
			ATF_REQUIRE_MSG(
			    found_set_bit == i,
			    "bit_nset_%d_%d_%d%s: Failed with result %d",
			    nbits, i, j, memloc, found_set_bit);

			bit_ffc_at(bitstr, i, nbits, &found_clear_bit);
			ATF_REQUIRE_MSG(
			    (j + 1 < nbits) ? found_clear_bit == j + 1 : -1,
			    "bit_nset_%d_%d_%d%s: Failed with result %d",
			    nbits, i, j, memloc, found_clear_bit);
		}
	}
}

BITSTRING_TC_DEFINE(bit_count)
/* bitstr_t *bitstr, int nbits, const char *memloc */
{
	int result, s, e, expected;

	/* Empty bitstr */
	memset(bitstr, 0, bitstr_size(nbits));
	bit_count(bitstr, 0, nbits, &result);
	ATF_CHECK_MSG(0 == result,
			"bit_count_%d_%s_%s: Failed with result %d",
			nbits, "clear", memloc, result);

	/* Full bitstr */
	memset(bitstr, 0xFF, bitstr_size(nbits));
	bit_count(bitstr, 0, nbits, &result);
	ATF_CHECK_MSG(nbits == result,
			"bit_count_%d_%s_%s: Failed with result %d",
			nbits, "set", memloc, result);

	/* Invalid _start value */
	memset(bitstr, 0xFF, bitstr_size(nbits));
	bit_count(bitstr, nbits, nbits, &result);
	ATF_CHECK_MSG(0 == result,
			"bit_count_%d_%s_%s: Failed with result %d",
			nbits, "invalid_start", memloc, result);
	
	/* Alternating bitstr, starts with 0 */
	memset(bitstr, 0xAA, bitstr_size(nbits));
	bit_count(bitstr, 0, nbits, &result);
	ATF_CHECK_MSG(nbits / 2 == result,
			"bit_count_%d_%s_%d_%s: Failed with result %d",
			nbits, "alternating", 0, memloc, result);

	/* Alternating bitstr, starts with 1 */
	memset(bitstr, 0x55, bitstr_size(nbits));
	bit_count(bitstr, 0, nbits, &result);
	ATF_CHECK_MSG((nbits + 1) / 2 == result,
			"bit_count_%d_%s_%d_%s: Failed with result %d",
			nbits, "alternating", 1, memloc, result);

	/* Varying start location */
	memset(bitstr, 0xAA, bitstr_size(nbits));
	for (s = 0; s < nbits; s++) {
		expected = s % 2 == 0 ? (nbits - s) / 2 : (nbits - s + 1) / 2;
		bit_count(bitstr, s, nbits, &result);
		ATF_CHECK_MSG(expected == result,
				"bit_count_%d_%s_%d_%s: Failed with result %d",
				nbits, "vary_start", s, memloc, result);
	}

	/* Varying end location */
	memset(bitstr, 0xAA, bitstr_size(nbits));
	for (e = 0; e < nbits; e++) {
		bit_count(bitstr, 0, e, &result);
		ATF_CHECK_MSG(e / 2 == result,
				"bit_count_%d_%s_%d_%s: Failed with result %d",
				nbits, "vary_end", e, memloc, result);
	}

}

BITSTRING_TC_DEFINE(bit_foreach)
/* bitstr_t *bitstr, int nbits, const char *memloc */
{
	int i, set_bit;

	/* Empty bitstr */
	memset(bitstr, 0x00, bitstr_size(nbits));
	bit_foreach (bitstr, nbits, set_bit) {
		atf_tc_fail("bit_foreach_%d_%s_%s: Failed at location %d",
		    nbits, "clear", memloc, set_bit);
	}

	/* Full bitstr */
	i = 0;
	memset(bitstr, 0xFF, bitstr_size(nbits));
	bit_foreach(bitstr, nbits, set_bit) {
		ATF_REQUIRE_MSG(set_bit == i,
		    "bit_foreach_%d_%s_%s: Failed on turn %d at location %d",
		    nbits, "set", memloc, i, set_bit);
		i++;
	}
	ATF_REQUIRE_MSG(i == nbits,
	    "bit_foreach_%d_%s_%s: Invalid number of turns %d",
	    nbits, "set", memloc, i);

	/* Alternating bitstr, starts with 0 */
	i = 0;
	memset(bitstr, 0xAA, bitstr_size(nbits));
	bit_foreach(bitstr, nbits, set_bit) {
		ATF_REQUIRE_MSG(set_bit == i * 2 + 1,
		    "bit_foreach_%d_%s_%d_%s: "
		    "Failed on turn %d at location %d",
		    nbits, "alternating", 0,  memloc, i, set_bit);
		i++;
	}
	ATF_REQUIRE_MSG(i == nbits / 2,
	    "bit_foreach_%d_%s_%d_%s: Invalid number of turns %d",
	    nbits, "alternating", 0, memloc, i);

	/* Alternating bitstr, starts with 1 */
	i = 0;
	memset(bitstr, 0x55, bitstr_size(nbits));
	bit_foreach(bitstr, nbits, set_bit) {
		ATF_REQUIRE_MSG(set_bit == i * 2,
		    "bit_foreach_%d_%s_%d_%s: "
		    "Failed on turn %d at location %d",
		    nbits, "alternating", 1, memloc, i, set_bit);
		i++;
	}
	ATF_REQUIRE_MSG(i == (nbits + 1) / 2,
	    "bit_foreach_%d_%s_%d_%s: Invalid number of turns %d",
	    nbits, "alternating", 1, memloc, i);
}

BITSTRING_TC_DEFINE(bit_foreach_at)
/* bitstr_t *bitstr, int nbits, const char *memloc */
{
	int i, s, e, set_bit;

	/* Invalid _start value */
	memset(bitstr, 0xFF, bitstr_size(nbits));
	bit_foreach_at(bitstr, nbits, nbits, set_bit) {
		atf_tc_fail("bit_foreach_at_%d_%s_%s: Failed at location %d",
		    nbits, "invalid_start", memloc, set_bit);
	}

	/* Varying start location */
	memset(bitstr, 0xAA, bitstr_size(nbits));
	for (s = 0; s < nbits; s++) {
		i = 0;
		bit_foreach_at(bitstr, s, nbits, set_bit) {
			ATF_REQUIRE_MSG(set_bit == (i + s / 2) * 2 + 1,
			    "bit_foreach_at_%d_%s_%d_%s: "
			    "Failed on turn %d at location %d",
			    nbits, "vary_start", s,  memloc, i, set_bit);
			i++;
		}
		ATF_REQUIRE_MSG(i == nbits / 2 - s / 2,
		    "bit_foreach_at_%d_%s_%d_%s: Invalid number of turns %d",
		    nbits, "vary_start", s, memloc, i);
	}

	/* Varying end location */
	memset(bitstr, 0xAA, bitstr_size(nbits));
	for (e = 0; e < nbits; e++) {
		i = 0;
		bit_foreach_at(bitstr, 0, e, set_bit) {
			ATF_REQUIRE_MSG(set_bit == i * 2 + 1,
			    "bit_foreach_at_%d_%s_%d_%s: "
			    "Failed on turn %d at location %d",
			    nbits, "vary_end", e,  memloc, i, set_bit);
			i++;
		}
		ATF_REQUIRE_MSG(i == e / 2,
		    "bit_foreach_at_%d_%s_%d_%s: Invalid number of turns %d",
		    nbits, "vary_end", e, memloc, i);
	}
}

BITSTRING_TC_DEFINE(bit_foreach_unset)
/* bitstr_t *bitstr, int nbits, const char *memloc */
{
	int i, unset_bit;

	/* Empty bitstr */
	i = 0;
	memset(bitstr, 0, bitstr_size(nbits));
	bit_foreach_unset(bitstr, nbits, unset_bit) {
		ATF_REQUIRE_MSG(unset_bit == i,
		    "bit_foreach_unset_%d_%s_%s: "
		    "Failed on turn %d at location %d",
		    nbits, "clear", memloc, i, unset_bit);
		i++;
	}
	ATF_REQUIRE_MSG(i == nbits,
	    "bit_foreach_unset_%d_%s_%s: Invalid number of turns %d",
	    nbits, "set", memloc, i);

	/* Full bitstr */
	memset(bitstr, 0xFF, bitstr_size(nbits));
	bit_foreach_unset(bitstr, nbits, unset_bit) {
		atf_tc_fail("bit_foreach_unset_%d_%s_%s: "
		    "Failed at location %d",
		    nbits, "set", memloc, unset_bit);
	}

	/* Alternating bitstr, starts with 0 */
	i = 0;
	memset(bitstr, 0xAA, bitstr_size(nbits));
	bit_foreach_unset(bitstr, nbits, unset_bit) {
		ATF_REQUIRE_MSG(unset_bit == i * 2,
		    "bit_foreach_unset_%d_%s_%d_%s: "
		    "Failed on turn %d at location %d",
		    nbits, "alternating", 0,  memloc, i, unset_bit);
		i++;
	}
	ATF_REQUIRE_MSG(i == (nbits + 1) / 2,
	    "bit_foreach_unset_%d_%s_%d_%s: Invalid number of turns %d",
	    nbits, "alternating", 0, memloc, i);

	/* Alternating bitstr, starts with 1 */
	i = 0;
	memset(bitstr, 0x55, bitstr_size(nbits));
	bit_foreach_unset(bitstr, nbits, unset_bit) {
		ATF_REQUIRE_MSG(unset_bit == i * 2 + 1,
		    "bit_foreach_unset_%d_%s_%d_%s: "
		    "Failed on turn %d at location %d",
		    nbits, "alternating", 1, memloc, i, unset_bit);
		i++;
	}
	ATF_REQUIRE_MSG(i == nbits / 2,
	    "bit_foreach_unset_%d_%s_%d_%s: Invalid number of turns %d",
	    nbits, "alternating", 1, memloc, i);
}

BITSTRING_TC_DEFINE(bit_foreach_unset_at)
/* bitstr_t *bitstr, int nbits, const char *memloc */
{
	int i, s, e, unset_bit;

	/* Invalid _start value */
	memset(bitstr, 0, bitstr_size(nbits));
	bit_foreach_unset_at(bitstr, nbits, nbits, unset_bit) {
		atf_tc_fail("bit_foreach_unset_at_%d_%s_%s: "
		    "Failed at location %d",
		    nbits, "invalid_start", memloc, unset_bit);
	}

	/* Varying start location */
	memset(bitstr, 0xAA, bitstr_size(nbits));
	for (s = 0; s < nbits; s++) {
		i = 0;
		bit_foreach_unset_at(bitstr, s, nbits, unset_bit) {
			ATF_REQUIRE_MSG(unset_bit == (i + (s + 1) / 2) * 2,
			    "bit_foreach_unset_at_%d_%s_%d_%s: "
			    "Failed on turn %d at location %d",
			    nbits, "vary_start", s,  memloc, i, unset_bit);
			i++;
		}
		ATF_REQUIRE_MSG(i == (nbits + 1) / 2 - (s + 1) / 2,
		    "bit_foreach_unset_at_%d_%s_%d_%s: "
		    "Invalid number of turns %d",
		    nbits, "vary_start", s, memloc, i);
	}

	/* Varying end location */
	memset(bitstr, 0xAA, bitstr_size(nbits));
	for (e = 0; e < nbits; e++) {
		i = 0;
		bit_foreach_unset_at(bitstr, 0, e, unset_bit) {
			ATF_REQUIRE_MSG(unset_bit == i * 2,
			    "bit_foreach_unset_at_%d_%s_%d_%s: "
			    "Failed on turn %d at location %d",
			    nbits, "vary_end", e,  memloc, i, unset_bit);
			i++;
		}
		ATF_REQUIRE_MSG(i == (e + 1) / 2,
		    "bit_foreach_unset_at_%d_%s_%d_%s: "
		    "Invalid number of turns %d",
		    nbits, "vary_end", e, memloc, i);
	}
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, bitstr_in_struct);
	ATF_TP_ADD_TC(tp, bitstr_size);
	ATF_TP_ADD_TC(tp, bit_ffc_area);
	ATF_TP_ADD_TC(tp, bit_ffs_area);
	BITSTRING_TC_ADD(tp, bit_set);
	BITSTRING_TC_ADD(tp, bit_clear);
	BITSTRING_TC_ADD(tp, bit_ffs);
	BITSTRING_TC_ADD(tp, bit_ffc);
	BITSTRING_TC_ADD(tp, bit_ffs_at);
	BITSTRING_TC_ADD(tp, bit_ffc_at);
	BITSTRING_TC_ADD(tp, bit_nclear);
	BITSTRING_TC_ADD(tp, bit_nset);
	BITSTRING_TC_ADD(tp, bit_count);
	BITSTRING_TC_ADD(tp, bit_ffs_area_at_all_or_nothing);
	BITSTRING_TC_ADD(tp, bit_ffc_area_at_all_or_nothing);
	BITSTRING_TC_ADD(tp, bit_foreach);
	BITSTRING_TC_ADD(tp, bit_foreach_at);
	BITSTRING_TC_ADD(tp, bit_foreach_unset);
	BITSTRING_TC_ADD(tp, bit_foreach_unset_at);

	return (atf_no_error());
}

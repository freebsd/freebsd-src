/*	$NetBSD: t_bitops.c,v 1.16 2012/12/07 02:28:19 christos Exp $ */

/*-
 * Copyright (c) 2011, 2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas and Jukka Ruohonen.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__RCSID("$NetBSD: t_bitops.c,v 1.16 2012/12/07 02:28:19 christos Exp $");

#include <atf-c.h>

#include <sys/cdefs.h>
#include <sys/bitops.h>

#include <math.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

static const struct {
	uint32_t	val;
	int		ffs;
	int		fls;
} bits[] = {

	{ 0x00, 0, 0 }, { 0x01, 1, 1 },	{ 0x02, 2, 2 },	{ 0x03, 1, 2 },
	{ 0x04, 3, 3 }, { 0x05, 1, 3 },	{ 0x06, 2, 3 },	{ 0x07, 1, 3 },
	{ 0x08, 4, 4 }, { 0x09, 1, 4 },	{ 0x0A, 2, 4 },	{ 0x0B, 1, 4 },
	{ 0x0C, 3, 4 }, { 0x0D, 1, 4 },	{ 0x0E, 2, 4 },	{ 0x0F, 1, 4 },

	{ 0x10, 5, 5 },	{ 0x11, 1, 5 },	{ 0x12, 2, 5 },	{ 0x13, 1, 5 },
	{ 0x14, 3, 5 },	{ 0x15, 1, 5 },	{ 0x16, 2, 5 },	{ 0x17, 1, 5 },
	{ 0x18, 4, 5 },	{ 0x19, 1, 5 },	{ 0x1A, 2, 5 },	{ 0x1B, 1, 5 },
	{ 0x1C, 3, 5 },	{ 0x1D, 1, 5 },	{ 0x1E, 2, 5 },	{ 0x1F, 1, 5 },

	{ 0xF0, 5, 8 },	{ 0xF1, 1, 8 },	{ 0xF2, 2, 8 },	{ 0xF3, 1, 8 },
	{ 0xF4, 3, 8 },	{ 0xF5, 1, 8 },	{ 0xF6, 2, 8 },	{ 0xF7, 1, 8 },
	{ 0xF8, 4, 8 },	{ 0xF9, 1, 8 },	{ 0xFA, 2, 8 },	{ 0xFB, 1, 8 },
	{ 0xFC, 3, 8 },	{ 0xFD, 1, 8 },	{ 0xFE, 2, 8 },	{ 0xFF, 1, 8 },

};

ATF_TC(bitmap_basic);
ATF_TC_HEAD(bitmap_basic, tc)
{
        atf_tc_set_md_var(tc, "descr", "A basic test of __BITMAP_*");
}

ATF_TC_BODY(bitmap_basic, tc)
{
	__BITMAP_TYPE(, uint32_t, 65536) bm;
	__BITMAP_ZERO(&bm);

	ATF_REQUIRE(__BITMAP_SIZE(uint32_t, 65536) == 2048);

	ATF_REQUIRE(__BITMAP_SHIFT(uint32_t) == 5);

	ATF_REQUIRE(__BITMAP_MASK(uint32_t) == 31);

	for (size_t i = 0; i < 65536; i += 2)
		__BITMAP_SET(i, &bm);

	for (size_t i = 0; i < 2048; i++)
		ATF_REQUIRE(bm._b[i] == 0x55555555);

	for (size_t i = 0; i < 65536; i++)
		if (i & 1)
			ATF_REQUIRE(!__BITMAP_ISSET(i, &bm));
		else {
			ATF_REQUIRE(__BITMAP_ISSET(i, &bm));
			__BITMAP_CLR(i, &bm);
		}

	for (size_t i = 0; i < 65536; i += 2)
		ATF_REQUIRE(!__BITMAP_ISSET(i, &bm));
}

ATF_TC(fast_divide32);
ATF_TC_HEAD(fast_divide32, tc)
{
	atf_tc_set_md_var(tc, "descr", "A basic test of fast_divide32(3)");
}

ATF_TC_BODY(fast_divide32, tc)
{
	uint32_t a, b, q, r, m;
	uint8_t i, s1, s2;

	a = 0xFFFF;
	b = 0x000F;

	fast_divide32_prepare(b, &m, &s1, &s2);

	q = fast_divide32(a, b, m, s1, s2);
	r = fast_remainder32(a, b, m, s1, s2);

	ATF_REQUIRE(q == 0x1111 && r == 0);

	for (i = 1; i < __arraycount(bits); i++) {

		a = bits[i].val;
		b = bits[i].ffs;

		fast_divide32_prepare(b, &m, &s1, &s2);

		q = fast_divide32(a, b, m, s1, s2);
		r = fast_remainder32(a, b, m, s1, s2);

		ATF_REQUIRE(q == a / b);
		ATF_REQUIRE(r == a % b);
	}
}

ATF_TC(ffsfls);
ATF_TC_HEAD(ffsfls, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test ffs32(3)-family for correctness");
}

ATF_TC_BODY(ffsfls, tc)
{
	uint8_t i;

	ATF_REQUIRE(ffs32(0) == 0x00);
	ATF_REQUIRE(fls32(0) == 0x00);
	ATF_REQUIRE(ffs64(0) == 0x00);
	ATF_REQUIRE(fls64(0) == 0x00);

	ATF_REQUIRE(ffs32(UINT32_MAX) == 0x01);
	ATF_REQUIRE(fls32(UINT32_MAX) == 0x20);
	ATF_REQUIRE(ffs64(UINT64_MAX) == 0x01);
	ATF_REQUIRE(fls64(UINT64_MAX) == 0x40);

	for (i = 1; i < __arraycount(bits); i++) {

		ATF_REQUIRE(ffs32(bits[i].val) == bits[i].ffs);
		ATF_REQUIRE(fls32(bits[i].val) == bits[i].fls);
		ATF_REQUIRE(ffs64(bits[i].val) == bits[i].ffs);
		ATF_REQUIRE(fls64(bits[i].val) == bits[i].fls);

		ATF_REQUIRE(ffs32(bits[i].val << 1) == bits[i].ffs + 1);
		ATF_REQUIRE(fls32(bits[i].val << 1) == bits[i].fls + 1);
		ATF_REQUIRE(ffs64(bits[i].val << 1) == bits[i].ffs + 1);
		ATF_REQUIRE(fls64(bits[i].val << 1) == bits[i].fls + 1);

		ATF_REQUIRE(ffs32(bits[i].val << 9) == bits[i].ffs + 9);
		ATF_REQUIRE(fls32(bits[i].val << 9) == bits[i].fls + 9);
		ATF_REQUIRE(ffs64(bits[i].val << 9) == bits[i].ffs + 9);
		ATF_REQUIRE(fls64(bits[i].val << 9) == bits[i].fls + 9);
	}
}

ATF_TC(ilog2_basic);
ATF_TC_HEAD(ilog2_basic, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test ilog2(3) for correctness");
}

ATF_TC_BODY(ilog2_basic, tc)
{
	uint64_t i, x;

	for (i = x = 0; i < 64; i++) {

		x = (uint64_t)1 << i;

		ATF_REQUIRE(i == (uint64_t)ilog2(x));
	}
}

ATF_TC(ilog2_log2);
ATF_TC_HEAD(ilog2_log2, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test log2(3) vs. ilog2(3)");
}

ATF_TC_BODY(ilog2_log2, tc)
{
#ifdef __vax__
	atf_tc_skip("Test is unavailable on vax because of lack of log2()");
#else
	double  x, y;
	uint64_t i;

	/*
	 * This may fail under QEMU; see PR misc/44767.
	 */
	for (i = 1; i < UINT32_MAX; i += UINT16_MAX) {

		x = log2(i);
		y = (double)(ilog2(i));

		ATF_REQUIRE(ceil(x) >= y);

		if (fabs(floor(x) - y) > 1.0e-40) {
			atf_tc_expect_fail("PR misc/44767");
			atf_tc_fail("log2(%"PRIu64") != "
			    "ilog2(%"PRIu64")", i, i);
		}
	}
#endif
}

ATF_TP_ADD_TCS(tp)
{

        ATF_TP_ADD_TC(tp, bitmap_basic);
	ATF_TP_ADD_TC(tp, fast_divide32);
	ATF_TP_ADD_TC(tp, ffsfls);
	ATF_TP_ADD_TC(tp, ilog2_basic);
	ATF_TP_ADD_TC(tp, ilog2_log2);

	return atf_no_error();
}

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Conrad Meyer <cem@FreeBSD.org>
 * All rights reserved.
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
#include <sys/random.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include <crypto/chacha20/chacha.h>
#include <crypto/rijndael/rijndael-api-fst.h>
#include <crypto/sha2/sha256.h>

#include <dev/random/hash.h>
#include <dev/random/uint128.h>

#include <atf-c.h>

static void
vec_u32_tole128(uint8_t dst[static 16], const uint32_t src[static 4])
{
	le32enc(dst, src[0]);
	le32enc(&dst[4], src[1]);
	le32enc(&dst[8], src[2]);
	le32enc(&dst[12], src[3]);
}

static void
le128_to_vec_u32(uint32_t dst[static 4], const uint8_t src[static 16])
{
	dst[0] = le32dec(src);
	dst[1] = le32dec(&src[4]);
	dst[2] = le32dec(&src[8]);
	dst[3] = le32dec(&src[12]);
}

static void
formatu128(char buf[static 52], uint128_t x)
{
	uint8_t le128x[16];
	uint32_t vx[4];
	size_t sz, i;
	int rc;

	le128enc(le128x, x);
	le128_to_vec_u32(vx, le128x);

	sz = 52;
	for (i = 0; i < 4; i++) {
		rc = snprintf(buf, sz, "0x%x ", vx[i]);
		ATF_REQUIRE(rc > 0 && (size_t)rc < sz);

		buf += rc;
		sz -= rc;
	}
	/* Delete last trailing space */
	buf[-1] = '\0';
}

static void
u128_check_equality(uint128_t a, uint128_t b, const char *descr)
{
	char fmtbufa[52], fmtbufb[52];

	formatu128(fmtbufa, a);
	formatu128(fmtbufb, b);

	ATF_CHECK_MSG(uint128_equals(a, b),
	    "Expected: [%s] != Actual: [%s]: %s", fmtbufa, fmtbufb, descr);
}

ATF_TC_WITHOUT_HEAD(uint128_inc);
ATF_TC_BODY(uint128_inc, tc)
{
	static const struct u128_inc_tc {
		uint32_t input[4];
		uint32_t expected[4];
		const char *descr;
	} tests[] = {
		{
			.input = { 0, 0, 0, 0 },
			.expected = { 1, 0, 0, 0 },
			.descr = "0 -> 1",
		},
		{
			.input = { 1, 0, 0, 0 },
			.expected = { 2, 0, 0, 0 },
			.descr = "0 -> 2",
		},
		{
			.input = { 0xff, 0, 0, 0 },
			.expected = { 0x100, 0, 0, 0 },
			.descr = "0xff -> 0x100 (byte carry)",
		},
		{
			.input = { UINT32_MAX, 0, 0, 0 },
			.expected = { 0, 1, 0, 0 },
			.descr = "2^32 - 1 -> 2^32 (word carry)",
		},
		{
			.input = { UINT32_MAX, UINT32_MAX, 0, 0 },
			.expected = { 0, 0, 1, 0 },
			.descr = "2^64 - 1 -> 2^64 (u128t_word0 carry)",
		},
		{
			.input = { UINT32_MAX, UINT32_MAX, UINT32_MAX, 0 },
			.expected = { 0, 0, 0, 1 },
			.descr = "2^96 - 1 -> 2^96 (word carry)",
		},
	};
	uint8_t inputle[16], expectedle[16];
	uint128_t a;
	size_t i;

	for (i = 0; i < nitems(tests); i++) {
		vec_u32_tole128(inputle, tests[i].input);
		vec_u32_tole128(expectedle, tests[i].expected);

		a = le128dec(inputle);
		uint128_increment(&a);
		u128_check_equality(le128dec(expectedle), a, tests[i].descr);
	}
}

ATF_TC_WITHOUT_HEAD(uint128_add64);
ATF_TC_BODY(uint128_add64, tc)
{
	static const struct u128_add64_tc {
		uint32_t input[4];
		uint64_t addend;
		uint32_t expected[4];
		const char *descr;
	} tests[] = {
		{
			.input = { 0, 0, 0, 0 },
			.addend = 1,
			.expected = { 1, 0, 0, 0 },
			.descr = "0 + 1 -> 1",
		},
		{
			.input = { 1, 0, 0, 0 },
			.addend = UINT32_MAX,
			.expected = { 0, 1, 0, 0 },
			.descr = "1 + (2^32 - 1) -> 2^32 (word carry)",
		},
		{
			.input = { 1, 0, 0, 0 },
			.addend = UINT64_MAX,
			.expected = { 0, 0, 1, 0 },
			.descr = "1 + (2^64 - 1) -> 2^64 (u128t_word0 carry)",
		},
		{
			.input = { 0x11111111, 0x11111111, 0, 0 },
			.addend = 0xf0123456789abcdeULL,
			.expected = { 0x89abcdef, 0x01234567, 1, 0 },
			.descr = "0x1111_1111_1111_1111 +"
				 "0xf012_3456_789a_bcde ->"
			       "0x1_0123_4567_89ab_cdef",
		},
		{
			.input = { 1, 0, UINT32_MAX, 0 },
			.addend = UINT64_MAX,
			.expected = { 0, 0, 0, 1 },
			.descr = "Carry ~2^96",
		},
	};
	uint8_t inputle[16], expectedle[16];
	uint128_t a;
	size_t i;

	for (i = 0; i < nitems(tests); i++) {
		vec_u32_tole128(inputle, tests[i].input);
		vec_u32_tole128(expectedle, tests[i].expected);

		a = le128dec(inputle);
		uint128_add64(&a, tests[i].addend);
		u128_check_equality(le128dec(expectedle), a, tests[i].descr);
	}
}

/*
 * Test assumptions about Chacha incrementing counter in the same way as
 * uint128.h
 */
ATF_TC_WITHOUT_HEAD(uint128_chacha_ctr);
ATF_TC_BODY(uint128_chacha_ctr, tc)
{
	static const struct u128_chacha_tc {
		uint32_t input[4];
		uint32_t expected[4];
		const char *descr;
	} tests[] = {
		{
			.input = { 0, 0, 0, 0 },
			.expected = { 1, 0, 0, 0 },
			.descr = "Single block",
		},
		{
			.input = { 1, 0, 0, 0 },
			.expected = { 2, 0, 0, 0 },
			.descr = "0 -> 2",
		},
		{
			.input = { 0xff, 0, 0, 0 },
			.expected = { 0x100, 0, 0, 0 },
			.descr = "0xff -> 0x100 (byte carry)",
		},
		{
			.input = { UINT32_MAX, 0, 0, 0 },
			.expected = { 0, 1, 0, 0 },
			.descr = "2^32 - 1 -> 2^32 (word carry)",
		},
		{
			.input = { UINT32_MAX, UINT32_MAX, 0, 0 },
			.expected = { 0, 0, 1, 0 },
			.descr = "2^64 - 1 -> 2^64 (u128t_word0 carry)",
		},
		{
			.input = { UINT32_MAX, UINT32_MAX, UINT32_MAX, 0 },
			.expected = { 0, 0, 0, 1 },
			.descr = "2^96 - 1 -> 2^96 (word carry)",
		},
	};
	union randomdev_key context;
	uint8_t inputle[16], expectedle[16], trash[CHACHA_BLOCKLEN];
	uint8_t notrandomkey[RANDOM_KEYSIZE] = { 0 };
	uint128_t a;
	size_t i;

	random_chachamode = true;
	randomdev_encrypt_init(&context, notrandomkey);

	for (i = 0; i < nitems(tests); i++) {
		vec_u32_tole128(inputle, tests[i].input);
		vec_u32_tole128(expectedle, tests[i].expected);

		a = le128dec(inputle);
		randomdev_keystream(&context, &a, trash, sizeof(trash));
		u128_check_equality(le128dec(expectedle), a, tests[i].descr);
	}

}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, uint128_inc);
	ATF_TP_ADD_TC(tp, uint128_add64);
	ATF_TP_ADD_TC(tp, uint128_chacha_ctr);
	return (atf_no_error());
}

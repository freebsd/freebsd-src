/*
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This software was developed by Olivier Certner <olce@FreeBSD.org> at
 * Kumacom SARL under sponsorship from the FreeBSD Foundation.
 */

#include <sys/_param.h>	/* nitems() */
#define LIBKERN_TESTING
#include <sys/libkern.h>

#include <inttypes.h>
#include <stdint.h>

#include <atf-c.h>


static const struct sign_extend_test {
	uint64_t	input;
	int		sign_bit_idx;
	/*
	 * Unsigned result, as what interests us is the final bit representation
	 * and two-complement is always assumed.
	 */
	uint64_t	result;
} sign_extend_tests[] = {
	/* 32-bit input */
	{ 0x00010000, 15,  0 },
	{ 0x00010000, 16, -0x00010000 },
	{ 0x0f001000, 12, -0x1000 },
	{ 0x0f001000, 13,  0x1000 },
	{ 0x55555555,  0, -1 },
	{ 0x55555555,  1,  1 },
	{ 0x55555555, 11,  0x0555 },
	{ 0x55555555, 12, -0xaab },
	{ 0x55555555, 30, -0x2aaaaaab },
	{ 0x55555555, 31,  0x55555555 },
	{ 0xaaaaaaaa,  0,  0 },
	{ 0xaaaaaaaa,  1, -2 },
	{ 0xaaaaaaaa, 27, -0x05555556 },
	{ 0xaaaaaaaa, 28,  0x0aaaaaaa },
	{ 0xaaaaaaaa, 30,  0x2aaaaaaa },
	{ 0xaaaaaaaa, 31, -0x55555556 },
	/* 64-bit input */
	{ 0x5555555501234567,  0, -1 },
	{ 0x5555555501234567,  1, -1 },
	{ 0x5555555501234567, 31,  0x01234567 },
	{ 0x5555555501234567, 32,  0xffffffff01234567 },
	{ 0x5555555501234567, 45,  0x0000155501234567 },
	{ 0x5555555501234567, 46,  0xffffd55501234567 },
	{ 0x5555555501234567, 62,  0xd555555501234567 },
	{ 0x5555555501234567, 63,  0x5555555501234567 },
	{ 0xaaaaaaaa01234567,  0, -1 },
	{ 0xaaaaaaaa01234567,  1, -1 },
	{ 0xaaaaaaaa01234567, 31,  0x01234567 },
	{ 0xaaaaaaaa01234567, 32,  0x01234567 },
	{ 0xaaaaaaaa01234567, 55,  0xffaaaaaa01234567 },
	{ 0xaaaaaaaa01234567, 56,  0x00aaaaaa01234567 },
	{ 0xaaaaaaaa01234567, 62,  0x2aaaaaaa01234567 },
	{ 0xaaaaaaaa01234567, 63,  0xaaaaaaaa01234567 },
};

static const struct extract_signed_bitfield_test {
	uint64_t	input;
	int		lsb;
	int		width;
	/* Unsigned.  See comment in 'struct sign_extend_test'. */
	uint64_t	result;
} extract_signed_bitfield_tests[] = {
	/* 32-bit input */
	{ 0x00010000, 13,   3,  0 },
	{ 0x00010000, 13,   4, -8 },
	{ 0x0f001000, 22,   3, -4 },
	{ 0x0f001000, 22,   5, -4 },
	{ 0x0f001000, 26,   2, -1 },
	{ 0x0f001000, 26,   3,  3 },
	{ 0x00000005,  0,   1, -1 },
	{ 0x00000005,  0,   2,  1 },
	{ 0x00000005,  0,   3, -3 },
	{ 0x00000005,  1,   1,  0 },
	{ 0x00000005,  1,   2, -2 },
	{ 0x00005500, 11,   2, -2 },
	{ 0x00005500, 11,   3,  2 },
	{ 0x05555500, 11,  14, -0x1556 },
	{ 0x05555000, 12,  16,  0x5555 },
	{ 0x55555555, 29,   1,  0 },
	{ 0x55555555, 29,   2, -2 },
	{ 0x55555555, 29,   3,  2 },
	{ 0x0000000a,  0,   1,  0 },
	{ 0x0000000a,  0,   2, -2 },
	{ 0x0000000a,  1,   1, -1 },
	{ 0x0000000a,  1,   2,  1 },
	{ 0x000a0000, 18,   1,  0 },
	{ 0x000a0000, 18,   2, -2 },
	{ 0x00aa0000, 18,   3,  2 },
	{ 0x00aa0000, 18,   4, -6 },
	{ 0x0aaa0000, 18,   8, -0x56 },
	{ 0xaaaa0000, 18,  13,  0xaaa },
	{ 0xa0000000, 29,   1, -1 },
	{ 0xa0000000, 29,   2,  1 },
	{ 0xa0000000, 29,   3, -3 },
	/* 64-bit input */
	{ 0x00000005a5a00000, 21, 14, -0x12d3 },
	{ 0x00000005a5a00000, 21, 15,  0x2d2d },
	{ 0x0005500000000000, 45,  5,  0xa },
	{ 0x0005500000000000, 45,  6, -0x16 },
	{ 0x0005500000000000, 45,  7,  0x2a },
	{ 0x0012348765000000, 24,  32, 0x12348765 },
	{ 0xaa00000000000000, 58,  1,  0 },
	{ 0xaa00000000000000, 58,  2, -2 },
	{ 0xaa00000000000000, 58,  3,  2 },
	{ 0xaa00000000000000, 58,  4, -6 },
	{ 0xaa00000000000000, 58,  6, -22 },
};


ATF_TC_WITHOUT_HEAD(sign_extend);
ATF_TC_BODY(sign_extend, tc)
{
	for (size_t i = 0; i < nitems(sign_extend_tests); i++) {
		const struct sign_extend_test *const test =
		    sign_extend_tests + i;
		uint64_t cand;

		ATF_REQUIRE_MSG(0 <= test->sign_bit_idx &&
		    test->sign_bit_idx < 64,
		    "Wrong test case: Sign bit %d out of bounds\n",
		    test->sign_bit_idx);

		cand = sign_extend_64(test->input, test->sign_bit_idx);
		ATF_CHECK_EQ_MSG(test->result, cand,
		    "sign_extend_64(): Input %#" PRIx64 ", sign bit idx: %d, "
		    "expected: %#" PRIx64 " (%" PRId64 "), "
		    "got: %#" PRIx64 " (%" PRId64 ")\n",
		    test->input, test->sign_bit_idx, test->result, test->result,
		    cand, cand);

		/* Can that case be used to also test the 32-bit routine? */
		if (test->input <= UINT32_MAX && test->sign_bit_idx < 32) {
			cand = sign_extend_32(test->input, test->sign_bit_idx);
			ATF_CHECK_EQ_MSG(test->result, cand,
			    "sign_extend_32(): Input %#" PRIx64 ", "
			    "sign bit idx: %d, "
			    "expected: %#" PRIx32 " (%" PRId32 "), "
			    "got: %#" PRIx32 " (%" PRId32 ")\n",
			    test->input, test->sign_bit_idx,
			    (uint32_t)test->result, (uint32_t)test->result,
			    (uint32_t)cand, (uint32_t)cand);
		}
	}
}

ATF_TC_WITHOUT_HEAD(extract_signed_bitfield);
ATF_TC_BODY(extract_signed_bitfield, tc)
{
	for (size_t i = 0; i < nitems(extract_signed_bitfield_tests); i++) {
		const struct extract_signed_bitfield_test *const test =
		    extract_signed_bitfield_tests + i;
		uint64_t cand;

		ATF_REQUIRE_MSG(0 <= test->lsb && test->lsb < 64,
		    "Wrong test case: LSB index %d out of bounds\n",
		    test->lsb);
		ATF_REQUIRE_MSG(0 < test->width && test->lsb + test->width <= 64,
		    "Wrong test case: Width %d negative or too large for LSB\n",
		    test->width);

		cand = extract_signed_bitfield_64(test->input, test->lsb,
		    test->width);
		ATF_CHECK_EQ_MSG(test->result, cand,
		    "extract_signed_bitfield_64(): Input %#" PRIx64 ", "
		    "LSB idx: %d, width: %d, "
		    "expected: %#" PRIx64 " (%" PRId64 "), "
		    "got: %#" PRIx64 " (%" PRId64 ")\n",
		    test->input, test->lsb, test->width, test->result,
		    test->result, cand, cand);

		/* Can that case be used to also test the 32-bit routine? */
		if (test->input <= UINT32_MAX && test->lsb < 32 &&
		    test->lsb + test->width <= 32) {
			cand = extract_signed_bitfield_32(test->input,
			    test->lsb, test->width);
			ATF_CHECK_EQ_MSG(test->result, cand,
			    "extract_signed_bitfield_32(): Input %#" PRIx64 ", "
			    "LSB idx: %d, width: %d, "
			    "expected: %#" PRIx64 " (%" PRId64 "), "
			    "got: %#" PRIx64 " (%" PRId64 ")\n",
			    test->input, test->lsb, test->width, test->result,
			    test->result, cand, cand);
		}
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, sign_extend);
	ATF_TP_ADD_TC(tp, extract_signed_bitfield);

	return (atf_no_error());
}

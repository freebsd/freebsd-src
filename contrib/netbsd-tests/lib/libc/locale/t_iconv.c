/*	$NetBSD$	*/

/*-
 * Copyright (c) 2025 The NetBSD Foundation, Inc.
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

/*
 * iconv(3)
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: t_mbrtoc8.c,v 1.3 2024/08/20 17:43:09 riastradh Exp $");

#include <atf-c.h>
#include <errno.h>
#include <iconv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "h_macros.h"

static const struct sample {
	const char	*src_codeset;
	size_t		nsrc;
	const char	*src;
	const char	*dst_codeset;
	size_t		ndst;
	const char	*dst;
	size_t		ninval;
	size_t		ninval_rev;
	const char	*xfail;
#define	IRREVERSIBLE	((size_t)-1)
} samples[] = {
	[0] = { "us-ascii", 6, "hello",
		"us-ascii", 6, "hello",
		0, 0, NULL },
	/* LATIN SMALL LETTER E WITH ACUTE ACCENT */
	[1] = { "iso-8859-1", 1, (const char[1]){0xe9},
		"UTF-8", 2, (const char[2]){0xc3,0xa9},
		0, 0, NULL },
	/* LEFT CURLY BRACKET */
	[2] = { "UTF-8", 2, (const char[2]){0x00,0x7b},
		"ZW", 6, (const char[6]){0x00,0x7a,0x57,0x20,0x7b,0x0a},
		0, 0, NULL },
	/* LATIN SMALL LETTER O, TILDE */
	[3] = { "UTF-8", 2, (const char[2]){0x4f,0x7e},
		"VIQR", 3, (const char[3]){0x4f,0x5c,0x7e},
		0, 0, NULL },
	/* LEFT CURLY BRACKET (optionally encoded in UTF-7) */
	[4] = { "UTF-7", 1, (const char[1]){0x7b},
		"UTF-8", 1, (const char[1]){0x7b},
		0, IRREVERSIBLE, NULL },
	[5] = { "UTF-7", 4, "+AHs-",
		"UTF-8", 1, (const char[1]){0x7b},
		0, 0, NULL },
	/* éclair */
	[6] = { "UTF-7", 10, "+AOk-clair",
		"UTF-8", 7, (const char[7]){
			0xc3,0xa9,0x63,0x6c,0x61,0x69,0x72,
		},
		0, 0, NULL },
	[7] = { "HZ", 55,	/* RFC 1843, Sec. 4, Example 1 */
		"The next sentence is in GB.~{<:Ky2;S{#,NpJ)l6HK!#~}Bye.",
		"UTF-8", 61, (const char[61]) {
			0x54,0x68,0x65,0x20, 0x6e,0x65,0x78,0x74,
			0x20,0x73,0x65,0x6e, 0x74,0x65,0x6e,0x63,
			0x65,0x20,0x69,0x73, 0x20,0x69,0x6e,0x20,
			0x47,0x42,0x2e,0xe5, 0xb7,0xb1,0xe6,0x89,
			0x80,0xe4,0xb8,0x8d, 0xe6,0xac,0xb2,0xef,
			0xbc,0x8c,0xe5,0x8b, 0xbf,0xe6,0x96,0xbd,
			0xe6,0x96,0xbc,0xe4, 0xba,0xba,0xe3,0x80,
			0x82,0x42,0x79,0x65, 0x2e
		},
		0, 0, NULL },
	/* Same as above but with HZ8 instead of HZ. */
	[8] = { "HZ8", 55, (const char[55]) {
			0x54,0x68,0x65,0x20,0x6e,0x65,0x78,0x74,
			0x20,0x73,0x65,0x6e,0x74,0x65,0x6e,0x63,
			0x65,0x20,0x69,0x73,0x20,0x69,0x6e,0x20,
			0x47,0x42,0x2e,0x7e,0x7b,0xbc,0xba,0xcb,
			0xf9,0xb2,0xbb,0xd3,0xfb,0xa3,0xac,0xce,
			0xf0,0xca,0xa9,0xec,0xb6,0xc8,0xcb,0xa1,
			0xa3,0x7e,0x7d,0x42,0x79,0x65,0x2e
		},
		"UTF-8", 61, (const char[61]) {
			0x54,0x68,0x65,0x20, 0x6e,0x65,0x78,0x74,
			0x20,0x73,0x65,0x6e, 0x74,0x65,0x6e,0x63,
			0x65,0x20,0x69,0x73, 0x20,0x69,0x6e,0x20,
			0x47,0x42,0x2e,0xe5, 0xb7,0xb1,0xe6,0x89,
			0x80,0xe4,0xb8,0x8d, 0xe6,0xac,0xb2,0xef,
			0xbc,0x8c,0xe5,0x8b, 0xbf,0xe6,0x96,0xbd,
			0xe6,0x96,0xbc,0xe4, 0xba,0xba,0xe3,0x80,
			0x82,0x42,0x79,0x65, 0x2e
		},
		0, 0, NULL },
	/* 馬 from CNS 11643-1 */
	[9] = { "UTF-8", 3, (const char[3]){0xe9,0xa6,0xac},
		"ISO-2022-CN", 8,
		(const char[8]){0x1b,0x24,0x29,0x47,0x0e,0x58,0x6b,0x0f},
		0, 0, NULL },
	/* 馬毦 shifting from CNS 11643-1 to CNS 11643-2 */
	[10] = { "UTF-8", 6, (const char[6]){0xe9,0xa6,0xac,0xe6,0xaf,0xa6},
		"ISO-2022-CN", 16,
		(const char[16]){
			/* ESC $ ) G (shift G1 to CNS 11643 plane 1) */
			0x1b,0x24,0x29,0x47,
			0x0e,	   /* GL is G1 from now on */
			0x58,0x6b, /* 馬 */
			/* ESC $ * H (shift G2 to CNS 11643 plane 2) */
			0x1b,0x24,0x2a,0x48,
			0x1b,0x4e, /* GL is G2 for next char */
			0x30,0x21, /* 毦 */
			0x0f,	   /* GL is G0 from now on */
		},
		0, 0, "PR lib/59019: various iconv issues ([case 10: ISO-2022-CN to UTF-8 14/6] iconv: Illegal byte sequence (85))" },
};

#ifdef MIN
#undef MIN
#endif
#define	MIN(x, y)	((x) < (y) ? (x) : (y))

static void
test_sample_bounded(const char *title, const struct sample *S,
    size_t nsrc, size_t ndst)
{
	char inbuf[4096], inguard = arc4random();
	char outbuf[4096], outguard = arc4random();
	iconv_t C;
	char *src0, *src, *dst0, *dst;
	size_t srcleft0, srcleft, dstleft0, dstleft;
	size_t ninval;
	int error;

	ATF_REQUIRE_MSG(S->nsrc < sizeof(inbuf) - 2, "[%s]", title);
	ATF_REQUIRE_MSG(S->ndst < sizeof(outbuf) - 2, "[%s]", title);

	memset(inbuf, inguard, sizeof(inbuf));
	memset(outbuf, outguard, sizeof(outbuf));
	src = src0 = inbuf + 1;
	memcpy(src, S->src, nsrc);
	dst = dst0 = outbuf + 1;
	srcleft = srcleft0 = nsrc;
	dstleft = dstleft0 = ndst;

	C = iconv_open(S->dst_codeset, S->src_codeset);
	if (C == (iconv_t)-1) {
		error = errno;
		atf_tc_fail_nonfatal("[%s] iconv_open: %s (%d)", title,
		    strerror(error), error);
		return;
	}

	ninval = iconv(C, &src, &srcleft, &dst, &dstleft);
	if (ninval == (size_t)-1) {
		error = errno;
		/*
		 * Incomplete input manifests as EINVAL -- ignore that,
		 * unless we're trying the full-size input.
		 *
		 * Truncated output manifests as E2BIG -- ignore that,
		 * unless we're trying the full-size output.
		 */
		if ((error != EINVAL || nsrc == S->nsrc) &&
		    (error != E2BIG || ndst == S->ndst)) {
			atf_tc_fail_nonfatal("[%s] iconv: %s (%d)",
			    title, strerror(error), error);
			goto next;
		}
	} else if (ninval != S->ninval) {
		error = errno;
		atf_tc_fail_nonfatal("[%s] iconv:"
		    " %zu invalid, expected %zu",
		    title, ninval, S->ninval);
	}

	ATF_CHECK_MSG(src0 <= src && src <= src0 + srcleft0, "[%s] iconv:"
	    " src went from %p to %p, expected [%p,%p]",
	    title, src0, src, src0, src0 + srcleft0);
	ATF_CHECK_MSG(srcleft <= srcleft0, "[%s] iconv:"
	    " srcleft went from %zu to %zu",
	    title, srcleft0, srcleft);
	ATF_CHECK_MSG(dst0 <= dst && dst <= dst0 + dstleft0, "[%s] iconv:"
	    " dst went from %p to %p, expected [%p,%p]",
	    title, dst0, dst, dst0, dst0 + dstleft0);
	ATF_CHECK_MSG(dstleft <= dstleft0, "[%s] iconv:"
	    " dstleft went from %zu to %zu",
	    title, dstleft0, dstleft);
	if (memcmp(dst0, S->dst, MIN(ndst, dstleft0 - dstleft)) != 0) {
		size_t k;

		atf_tc_fail_nonfatal("[%s] iconv: bad conv", title);
		fprintf(stderr, "[%s] input:    ", title);
		for (k = 0; k < nsrc; k++)
			fprintf(stderr, " %02x", (unsigned char)S->src[k]);
		fprintf(stderr, "\n");
		fprintf(stderr, "[%s] expected: ", title);
		for (k = 0; k < ndst; k++)
			fprintf(stderr, " %02x", (unsigned char)S->dst[k]);
		fprintf(stderr, "\n");
		fprintf(stderr, "[%s] got:      ", title);
		for (k = 0; k < ndst; k++)
			fprintf(stderr, " %02x", (unsigned char)dst0[k]);
		fprintf(stderr, "\n");
	}

next:	if (dst0[-1] != outguard) {
		atf_tc_fail_nonfatal("[%s] iconv overran before buffer:"
		    " dst0[-1] = 0x%02x, expected 0x%02x",
		    title, dst0[-1], outguard);
	}
	if (dst0[ndst] != outguard) {
		atf_tc_fail_nonfatal("[%s] iconv overran after buffer:"
		    " dst0[ndst=%zu] = 0x%02x, expected 0x%02x",
		    title, ndst, dst0[ndst], outguard);
	}
	if (dst[0] != outguard) {
		atf_tc_fail_nonfatal("[%s] iconv overran past updated dst:"
		    " dst[0] = 0x%02x, expected 0x%02x",
		    title, dst[0], outguard);
	}

	if (iconv_close(C) == -1) {
		error = errno;
		atf_tc_fail_nonfatal("[%s] iconv_close: %s (%d)",
		    title, strerror(error), error);
	}
}

static void
test_sample(const char *title, const struct sample *S)
{
	char buf[128];
	size_t nsrc, ndst;

	fprintf(stderr, "test %s from %s to %s\n",
	    title, S->src_codeset, S->dst_codeset);

	for (nsrc = 0; nsrc <= S->nsrc; nsrc++) {
		snprintf(buf, sizeof(buf), "%s %zu/%zu", title, nsrc, S->ndst);
		test_sample_bounded(buf, S, nsrc, S->ndst);
	}

	for (ndst = 0; ndst <= S->ndst; ndst++) {
		snprintf(buf, sizeof(buf), "%s %zu/%zu", title, S->nsrc, ndst);
		test_sample_bounded(buf, S, S->nsrc, ndst);
	}
}

ATF_TC(iconv_samples);
ATF_TC_HEAD(iconv_samples, tc)
{

	atf_tc_set_md_var(tc, "descr", "Test iconv on various fixed samples");
}
ATF_TC_BODY(iconv_samples, tc)
{
	unsigned i;

	for (i = 0; i < __arraycount(samples); i++) {
		const struct sample *S = &samples[i];
		struct sample reverse = {
			.src_codeset = S->dst_codeset,
			.dst_codeset = S->src_codeset,
			.nsrc = S->ndst,
			.src = S->dst,
			.ndst = S->nsrc,
			.dst = S->src,
			.ninval = S->ninval_rev,
		};
		char buf[128];

		if (S->xfail)
			atf_tc_expect_fail("%s", S->xfail);

		snprintf(buf, sizeof(buf), "case %u: %s to %s", i,
		    S->src_codeset, S->dst_codeset);
		test_sample(buf, S);
		if (S->ninval_rev != IRREVERSIBLE) {
			snprintf(buf, sizeof(buf), "case %u: %s to %s", i,
			    S->dst_codeset, S->src_codeset);
			test_sample(buf, &reverse);
		}

		if (S->xfail)
			atf_tc_expect_pass();
	}
}

ATF_TC(iconv_pr59019_hz8);
ATF_TC_HEAD(iconv_pr59019_hz8, tc)
{

	atf_tc_set_md_var(tc, "descr", "Truncated HZ8 input from PR 59019");
}
ATF_TC_BODY(iconv_pr59019_hz8, tc)
{
	const char title[] = "iconv_pr59019_hz8";
	char in[4] = "\x7e\x7b\x7e\x7e";	/* ~{~~ */
	char out[4096], guard = arc4random();
	char *src0, *src, *dst0, *dst;
	size_t srcleft0, srcleft, dstleft0, dstleft;
	iconv_t C;
	size_t ninval;

	memset(out, guard, sizeof(out));

	REQUIRE_LIBC((C = iconv_open(/*to*/"UTF-8", /*from*/"HZ8")),
	    (iconv_t)-1);

	src = src0 = in;
	srcleft = srcleft0 = sizeof(in);
	dst = dst0 = out + 1;
	dstleft = dstleft0 = sizeof(out) - 2;

	ninval = iconv(C, &src, &srcleft, &dst, &dstleft);

	/*
	 * XXX Not actually 100% sure this is the correct result -- I
	 * can't find a reference for the HZ8 encoding.
	 */
	ATF_CHECK_EQ_MSG(ninval, (size_t)-1, "[%s] iconv: ninval=%zu", title,
	    ninval);

	ATF_CHECK_MSG(src0 <= src && src <= src0 + srcleft0, "[%s] iconv:"
	    " src went from %p to %p, expected [%p,%p)",
	    title, src0, src, src0, src0 + srcleft0);
	ATF_CHECK_MSG(srcleft <= srcleft0, "[%s] iconv:"
	    " srcleft went from %zu to %zu",
	    title, srcleft0, srcleft);
	ATF_CHECK_MSG(dst0 <= dst && dst <= dst0 + dstleft0, "[%s] iconv:"
	    " dst went from %p to %p, expected [%p,%p)",
	    title, dst0, dst, dst0, dst0 + dstleft0);
	ATF_CHECK_MSG(dstleft <= dstleft0, "[%s] iconv:"
	    " dstleft went from %zu to %zu",
	    title, dstleft0, dstleft);

	ATF_CHECK_EQ(dst0[-1], guard);
	ATF_CHECK_EQ(dst0[dstleft0], guard);

	RL(iconv_close(C));
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, iconv_pr59019_hz8);
	ATF_TP_ADD_TC(tp, iconv_samples);

	return atf_no_error();
}

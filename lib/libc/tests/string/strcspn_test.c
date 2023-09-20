/*-
 * Copyright (c) 2023 The FreeBSD Foundation
 *
 * This software was developed by Robert Clausecker <fuz@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ''AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE
 */

#include <sys/cdefs.h>

#include <atf-c.h>
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

enum {
	MAXALIGN = 16,	/* test all offsets from this alignment */
	MAXBUF = 64,	/* test up to this buffer length */
};

enum { NOMATCH, MATCH };

#ifdef STRSPN
#define STRXSPN strspn
#else
#define STRXSPN strcspn
#endif

static void
testcase(char *buf, size_t buflen, char *set, size_t setlen, int want_match)
{
	size_t i, outcome, expected;

	assert(setlen < UCHAR_MAX - 2);

	for (i = 0; i < buflen; i++)
#ifdef STRSPN
		buf[i] = UCHAR_MAX - i % (setlen > 0 ? setlen : 1);
#else /* strcspn */
		buf[i] = 1 + i % (UCHAR_MAX - setlen - 1);
#endif

	buf[i] = '\0';

	for (i = 0; i < setlen; i++)
		set[i] = UCHAR_MAX - i;

	set[i] = '\0';

#ifdef STRSPN
	if (setlen == 0)
		expected = 0;
	else if (want_match == MATCH && buflen > 0) {
		buf[buflen - 1] = 1;
		expected = buflen - 1;
	} else
		expected = buflen;
#else /* strcspn */
	if (want_match == MATCH && buflen > 0 && setlen > 0) {
		buf[buflen - 1] = UCHAR_MAX;
		expected = buflen - 1;
	} else
		expected = buflen;
#endif

	outcome = STRXSPN(buf, set);
	ATF_CHECK_EQ_MSG(expected, outcome, "%s(%p[%zu], %p[%zu]) = %zu != %zu",
	    __XSTRING(STRXSPN), buf, buflen, set, setlen, outcome, expected);
}

/* test set with all alignments and lengths of buf */
static void
test_buf_alignments(char *set, size_t setlen, int want_match)
{
	char buf[MAXALIGN + MAXBUF + 1];
	size_t i, j;

	for (i = 0; i < MAXALIGN; i++)
		for (j = 0; j <= MAXBUF; j++)
			testcase(buf + i, j, set, setlen, want_match);
}

/* test buf with all alignments and lengths of set */
static void
test_set_alignments(char *buf, size_t buflen, int want_match)
{
	char set[MAXALIGN + MAXBUF + 1];
	size_t i, j;

	for (i = 0; i < MAXALIGN; i++)
		for (j = 0; j <= MAXBUF; j++)
			testcase(buf, buflen, set + i, j, want_match);
}

ATF_TC_WITHOUT_HEAD(buf_alignments);
ATF_TC_BODY(buf_alignments, tc)
{
	char set[41];

	test_buf_alignments(set, 0, MATCH);
	test_buf_alignments(set, 1, MATCH);
	test_buf_alignments(set, 5, MATCH);
	test_buf_alignments(set, 20, MATCH);
	test_buf_alignments(set, 40, MATCH);

	test_buf_alignments(set, 0, NOMATCH);
	test_buf_alignments(set, 1, NOMATCH);
	test_buf_alignments(set, 5, NOMATCH);
	test_buf_alignments(set, 20, NOMATCH);
	test_buf_alignments(set, 40, NOMATCH);
}

ATF_TC_WITHOUT_HEAD(set_alignments);
ATF_TC_BODY(set_alignments, tc)
{
	char buf[31];

	test_set_alignments(buf,  0, MATCH);
	test_set_alignments(buf, 10, MATCH);
	test_set_alignments(buf, 20, MATCH);
	test_set_alignments(buf, 30, MATCH);

	test_set_alignments(buf,  0, NOMATCH);
	test_set_alignments(buf, 10, NOMATCH);
	test_set_alignments(buf, 20, NOMATCH);
	test_set_alignments(buf, 30, NOMATCH);
}

#ifndef STRSPN
/* test all positions in which set could match buf */
static void
test_match_positions(char *buf,  char *set, size_t buflen, size_t setlen)
{
	size_t i, j, outcome;

	memset(buf, '-', buflen);

	for (i = 0; i < setlen; i++)
		set[i] = 'A' + i;

	buf[buflen] = '\0';
	set[setlen] = '\0';

	/*
	 * Check for (mis)match at buffer position i
	 * against set position j.
	 */
	for (i = 0; i < buflen; i++) {
		for (j = 0; j < setlen; j++) {
			buf[i] = set[j];

			outcome = strcspn(buf, set);
			ATF_CHECK_EQ_MSG(i, outcome,
			    "strcspn(\"%s\", \"%s\") = %zu != %zu",
			    buf, set, outcome, i);
		}

		buf[i] = '-';
	}
}

ATF_TC_WITHOUT_HEAD(match_positions);
ATF_TC_BODY(match_positions, tc)
{
	char buf[129], set[65];

	test_match_positions(buf, set, 128, 64);
	test_match_positions(buf, set, 64, 64);
	test_match_positions(buf, set, 32, 64);
	test_match_positions(buf, set, 16, 64);
	test_match_positions(buf, set, 8, 64);
	test_match_positions(buf, set, 128, 32);
	test_match_positions(buf, set, 64, 32);
	test_match_positions(buf, set, 32, 32);
	test_match_positions(buf, set, 16, 32);
	test_match_positions(buf, set, 8, 32);
	test_match_positions(buf, set, 128, 16);
	test_match_positions(buf, set, 64, 16);
	test_match_positions(buf, set, 32, 16);
	test_match_positions(buf, set, 16, 16);
	test_match_positions(buf, set, 8, 16);
	test_match_positions(buf, set, 128, 8);
	test_match_positions(buf, set, 64, 8);
	test_match_positions(buf, set, 32, 8);
	test_match_positions(buf, set, 16, 8);
	test_match_positions(buf, set, 8, 8);
}
#endif /* !defined(STRSPN) */

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, buf_alignments);
	ATF_TP_ADD_TC(tp, set_alignments);
#ifndef STRSPN
	ATF_TP_ADD_TC(tp, match_positions);
#endif

	return (atf_no_error());
}

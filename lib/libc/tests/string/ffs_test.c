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

#include <atf-c.h>
#include <limits.h>
#include <stdint.h>
#include <strings.h>

#ifndef FFS
# define FFS ffs
# define TYPE int
# define TYPE_MIN INT_MIN
#endif

ATF_TC_WITHOUT_HEAD(zero);
ATF_TC_BODY(zero, tc)
{
	ATF_CHECK_EQ((TYPE)0, FFS(0));
}

ATF_TC_WITHOUT_HEAD(twobit);
ATF_TC_BODY(twobit, tc)
{
	const TYPE one = 1;
	TYPE x;
	const int n = sizeof(TYPE) * CHAR_BIT;
	int i, j;

	for (i = 0; i < n - 1; i++)
		for (j = 0; j <= i; j++) {
			x = one << i | one << j;
			ATF_CHECK_EQ_MSG(j + 1, FFS(x),
			    "%s(%#jx) == %d != %d", __STRING(FFS), (intmax_t)x, FFS(x), j + 1);
		}
}

ATF_TC_WITHOUT_HEAD(twobitneg);
ATF_TC_BODY(twobitneg, tc)
{
	const TYPE one = 1;
	TYPE x;
	const int n = sizeof(TYPE) * CHAR_BIT;
	int i, j;

	for (i = 0; i < n - 1; i++)
		for (j = 0; j <= i; j++) {
			x = one << i | one << j | TYPE_MIN;
			ATF_CHECK_EQ_MSG(j + 1, FFS(x),
			    "%s(%#jx) == %d != %d", __STRING(FFS), (intmax_t)x, FFS(x), j + 1);
		}
}


ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, zero);
	ATF_TP_ADD_TC(tp, twobit);
	ATF_TP_ADD_TC(tp, twobitneg);

	return (atf_no_error());
}

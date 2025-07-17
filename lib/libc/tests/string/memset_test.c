/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Strahinja Stanisic <strajabot@FreeBSD.org>
 */

#include <assert.h>
#include <string.h>

#include <atf-c.h>

ATF_TC_WITHOUT_HEAD(int_char_conv);
ATF_TC_BODY(int_char_conv, tc)
{
	char b[64];
	int c = 0xDEADBEEF;
	memset(&b, c, 64);
	for(int i = 0; i < 64; i++) {
		assert(b[i] == (char)c);
	}

}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, int_char_conv);
	return (atf_no_error());
}


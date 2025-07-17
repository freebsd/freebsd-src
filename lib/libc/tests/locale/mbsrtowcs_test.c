/*-
 * Copyright (c) 2002 Tim J. Robbins
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

/*
 * Test program for mbsrtowcs(), as specified by IEEE Std. 1003.1-2001 and
 * ISO/IEC 9899:1999.
 *
 * The function is tested with both the "C" ("POSIX") LC_CTYPE setting and
 * "ja_JP.eucJP". Other encodings are not tested.
 */

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <atf-c.h>

ATF_TC_WITHOUT_HEAD(mbsrtowcs_test);
ATF_TC_BODY(mbsrtowcs_test, tc)
{
	char srcbuf[128];
	wchar_t dstbuf[128];
	char *src;
	mbstate_t s;

	/*
	 * C/POSIX locale.
	 */

	/* Simple null terminated string. */
	memset(srcbuf, 0xcc, sizeof(srcbuf));
	strcpy(srcbuf, "hello");
	wmemset(dstbuf, 0xcccc, sizeof(dstbuf) / sizeof(*dstbuf));
	src = srcbuf;
	memset(&s, 0, sizeof(s));
	ATF_REQUIRE(mbsrtowcs(dstbuf, (const char **)&src, sizeof(dstbuf) /
	    sizeof(*dstbuf), &s) == 5);
	ATF_REQUIRE(wcscmp(dstbuf, L"hello") == 0);
	ATF_REQUIRE(dstbuf[6] == 0xcccc);
	ATF_REQUIRE(src == NULL);

	/* Not enough space in destination buffer. */
	memset(srcbuf, 0xcc, sizeof(srcbuf));
	strcpy(srcbuf, "hello");
	wmemset(dstbuf, 0xcccc, sizeof(dstbuf) / sizeof(*dstbuf));
	src = srcbuf;
	memset(&s, 0, sizeof(s));
	ATF_REQUIRE(mbsrtowcs(dstbuf, (const char **)&src, 4, &s) == 4);
	ATF_REQUIRE(wmemcmp(dstbuf, L"hell", 4) == 0);
	ATF_REQUIRE(dstbuf[5] == 0xcccc);
	ATF_REQUIRE(src == srcbuf + 4);

	/* Null terminated string, internal dest. buffer */
	memset(srcbuf, 0xcc, sizeof(srcbuf));
	strcpy(srcbuf, "hello");
	src = srcbuf;
	memset(&s, 0, sizeof(s));
	ATF_REQUIRE(mbsrtowcs(NULL, (const char **)&src, 0, &s) == 5);

	/* Null terminated string, internal state. */
	memset(srcbuf, 0xcc, sizeof(srcbuf));
	strcpy(srcbuf, "hello");
	wmemset(dstbuf, 0xcccc, sizeof(dstbuf) / sizeof(*dstbuf));
	src = srcbuf;
	ATF_REQUIRE(mbsrtowcs(dstbuf, (const char **)&src, sizeof(dstbuf) /
	    sizeof(*dstbuf), NULL) == 5);
	ATF_REQUIRE(wcscmp(dstbuf, L"hello") == 0);
	ATF_REQUIRE(dstbuf[6] == 0xcccc);
	ATF_REQUIRE(src == NULL);

	/* Null terminated string, internal state, internal dest. buffer. */
	memset(srcbuf, 0xcc, sizeof(srcbuf));
	strcpy(srcbuf, "hello");
	src = srcbuf;
	ATF_REQUIRE(mbsrtowcs(NULL, (const char **)&src, 0, NULL) == 5);

	/* Empty source buffer. */
	memset(srcbuf, 0xcc, sizeof(srcbuf));
	srcbuf[0] = '\0';
	src = srcbuf;
	memset(&s, 0, sizeof(s));
	wmemset(dstbuf, 0xcccc, sizeof(dstbuf) / sizeof(*dstbuf));
	ATF_REQUIRE(mbsrtowcs(dstbuf, (const char **)&src, 1, &s) == 0);
	ATF_REQUIRE(dstbuf[0] == 0);
	ATF_REQUIRE(dstbuf[1] == 0xcccc);
	ATF_REQUIRE(src == NULL);

	/* Zero length destination buffer. */
	memset(srcbuf, 0xcc, sizeof(srcbuf));
	strcpy(srcbuf, "hello");
	src = srcbuf;
	memset(&s, 0, sizeof(s));
	wmemset(dstbuf, 0xcccc, sizeof(dstbuf) / sizeof(*dstbuf));
	ATF_REQUIRE(mbsrtowcs(dstbuf, (const char **)&src, 0, &s) == 0);
	ATF_REQUIRE(dstbuf[0] == 0xcccc);
	ATF_REQUIRE(src == srcbuf);

	/*
	 * Japanese (EUC) locale.
	 */

	ATF_REQUIRE(strcmp(setlocale(LC_CTYPE, "ja_JP.eucJP"), "ja_JP.eucJP") == 0);
	ATF_REQUIRE(MB_CUR_MAX > 1);

	memset(srcbuf, 0xcc, sizeof(srcbuf));
	strcpy(srcbuf, "\xA3\xC1 B \xA3\xC3");
	src = srcbuf;
	memset(&s, 0, sizeof(s));
	wmemset(dstbuf, 0xcccc, sizeof(dstbuf) / sizeof(*dstbuf));
	ATF_REQUIRE(mbsrtowcs(dstbuf, (const char **)&src, sizeof(dstbuf) /
	    sizeof(*dstbuf), &s) == 5);
	ATF_REQUIRE(dstbuf[0] == 0xA3C1 && dstbuf[1] == 0x20 && dstbuf[2] == 0x42 &&
	    dstbuf[3] == 0x20 && dstbuf[4] == 0xA3C3 && dstbuf[5] == 0);
	ATF_REQUIRE(src == NULL);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, mbsrtowcs_test);

	return (atf_no_error());
}

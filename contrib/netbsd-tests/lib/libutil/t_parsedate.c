/* $NetBSD: t_parsedate.c,v 1.7 2013/01/19 15:21:43 apb Exp $ */
/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: t_parsedate.c,v 1.7 2013/01/19 15:21:43 apb Exp $");

#include <atf-c.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <util.h>

ATF_TC(dates);

ATF_TC_HEAD(dates, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test unambiguous dates"
	    " (PR lib/44255)");
}

ATF_TC_BODY(dates, tc)
{

	ATF_CHECK(parsedate("69-09-10", NULL, NULL) != -1);
	ATF_CHECK(parsedate("2006-11-17", NULL, NULL) != -1);
	ATF_CHECK(parsedate("10/1/2000", NULL, NULL) != -1);
	ATF_CHECK(parsedate("20 Jun 1994", NULL, NULL) != -1);
	ATF_CHECK(parsedate("23jun2001", NULL, NULL) != -1);
	ATF_CHECK(parsedate("1-sep-06", NULL, NULL) != -1);
	ATF_CHECK(parsedate("1/11", NULL, NULL) != -1);
	ATF_CHECK(parsedate("1500-01-02", NULL, NULL) != -1);
	ATF_CHECK(parsedate("9999-12-21", NULL, NULL) != -1);
}

ATF_TC(times);

ATF_TC_HEAD(times, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test times"
	    " (PR lib/44255)");
}

ATF_TC_BODY(times, tc)
{

	ATF_CHECK(parsedate("10:01", NULL, NULL) != -1);
	ATF_CHECK(parsedate("10:12pm", NULL, NULL) != -1);
	ATF_CHECK(parsedate("12:11:01.000012", NULL, NULL) != -1);
	ATF_CHECK(parsedate("12:21-0500", NULL, NULL) != -1);
}

ATF_TC(relative);

ATF_TC_HEAD(relative, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test relative items"
	    " (PR lib/44255)");
}

ATF_TC_BODY(relative, tc)
{

	ATF_CHECK(parsedate("-1 month", NULL, NULL) != -1);
	ATF_CHECK(parsedate("last friday", NULL, NULL) != -1);
	ATF_CHECK(parsedate("one week ago", NULL, NULL) != -1);
	ATF_CHECK(parsedate("this thursday", NULL, NULL) != -1);
	ATF_CHECK(parsedate("next sunday", NULL, NULL) != -1);
	ATF_CHECK(parsedate("+2 years", NULL, NULL) != -1);
}

ATF_TC(atsecs);

ATF_TC_HEAD(atsecs, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test seconds past the epoch");
}

ATF_TC_BODY(atsecs, tc)
{
	int tzoff;

	/* "@0" -> (time_t)0, regardless of timezone */
	ATF_CHECK(parsedate("@0", NULL, NULL) == (time_t)0);
	putenv(__UNCONST("TZ=Europe/Berlin"));
	tzset();
	ATF_CHECK(parsedate("@0", NULL, NULL) == (time_t)0);
	putenv(__UNCONST("TZ=America/New_York"));
	tzset();
	ATF_CHECK(parsedate("@0", NULL, NULL) == (time_t)0);
	tzoff = 0;
	ATF_CHECK(parsedate("@0", NULL, &tzoff) == (time_t)0);
	tzoff = 3600;
	ATF_CHECK(parsedate("@0", NULL, &tzoff) == (time_t)0);
	tzoff = -3600;
	ATF_CHECK(parsedate("@0", NULL, &tzoff) == (time_t)0);

	/* -1 or other negative numbers are not errors */
	errno = 0;
	ATF_CHECK(parsedate("@-1", NULL, &tzoff) == (time_t)-1 && errno == 0);
	ATF_CHECK(parsedate("@-2", NULL, &tzoff) == (time_t)-2 && errno == 0);

	/* junk is an error */
	errno = 0;
	ATF_CHECK(parsedate("@junk", NULL, NULL) == (time_t)-1 && errno != 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, dates);
	ATF_TP_ADD_TC(tp, times);
	ATF_TP_ADD_TC(tp, relative);
	ATF_TP_ADD_TC(tp, atsecs);

	return atf_no_error();
}

/*-
 * Copyright (C) 2025 ConnectWise, LLC. All rights reserved.
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

#include <sys/exterrvar.h>
#include <sys/mman.h>

#include <atf-c.h>
#include <errno.h>
#include <exterr.h>
#include <stdio.h>

ATF_TC(gettext_extended);
ATF_TC_HEAD(gettext_extended, tc)
{
	atf_tc_set_md_var(tc, "descr", "Retrieve an extended error message");
}
ATF_TC_BODY(gettext_extended, tc)
{
	char exterr[UEXTERROR_MAXLEN];
	int r;

	/*
	 * Use an invalid call to mmap() because it supports extended error
	 * messages, requires no special resources, and does not need root.
	 */
	ATF_CHECK_ERRNO(ENOTSUP,
	    mmap(NULL, 0, PROT_MAX(PROT_READ) | PROT_WRITE, 0, -1, 0));
	r = uexterr_gettext(exterr, sizeof(exterr));
	ATF_CHECK_EQ(0, r);
	printf("Extended error: %s\n", exterr);
	/* Note: error string may need to be updated due to kernel changes */
	ATF_CHECK(strstr(exterr, " is not subset of ") != 0);
}

ATF_TC(gettext_noextended);
ATF_TC_HEAD(gettext_noextended, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Fail to retrieve an extended error message because none exists");
}
ATF_TC_BODY(gettext_noextended, tc)
{
	char exterr[UEXTERROR_MAXLEN];
	int r;

	ATF_CHECK_ERRNO(EINVAL, exterrctl(EXTERRCTL_UD, 0, NULL));
	r = uexterr_gettext(exterr, sizeof(exterr));
	ATF_CHECK_EQ(0, r);
	ATF_CHECK_STREQ(exterr, "");
}

ATF_TC(gettext_noextended_after_extended);
ATF_TC_HEAD(gettext_noextended_after_extended, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "uexterr_gettext should not return a stale extended error message");
}
ATF_TC_BODY(gettext_noextended_after_extended, tc)
{
	char exterr[UEXTERROR_MAXLEN];
	int r;

	/*
	 * First do something that will create an extended error message, but
	 * ignore it.
	 */
	ATF_CHECK_ERRNO(ENOTSUP,
	    mmap(NULL, 0, PROT_MAX(PROT_READ) | PROT_WRITE, 0, -1, 0));

	/* Then do something that won't create an extended error message */
	ATF_CHECK_ERRNO(EINVAL, exterrctl(EXTERRCTL_UD, 0, NULL));

	/* Hopefully we won't see the stale extended error message */
	r = uexterr_gettext(exterr, sizeof(exterr));
	ATF_CHECK_EQ(0, r);
	ATF_CHECK_STREQ(exterr, "");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, gettext_extended);
	ATF_TP_ADD_TC(tp, gettext_noextended);
	ATF_TP_ADD_TC(tp, gettext_noextended_after_extended);

	return (atf_no_error());
}

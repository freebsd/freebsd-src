/* $NetBSD: t_fmod.c,v 1.4 2020/08/25 13:39:16 gson Exp $ */

/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Joerg Sonnenberger.
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

#include <atf-c.h>
#include <float.h>
#include <math.h>

#include "isqemu.h"

ATF_TC(fmod);
ATF_TC_HEAD(fmod, tc)
{
	atf_tc_set_md_var(tc, "descr","Check fmod family");
}

ATF_TC_BODY(fmod, tc)
{
#ifdef __NetBSD__
	if (isQEMU())
		atf_tc_expect_fail("PR misc/44767");
#endif

	ATF_CHECK(fmodf(2.0, 1.0) == 0);
	ATF_CHECK(fmod(2.0, 1.0) == 0);
	ATF_CHECK(fmodl(2.0, 1.0) == 0);

	ATF_CHECK(fmodf(2.0, 0.5) == 0);
	ATF_CHECK(fmod(2.0, 0.5) == 0);
	ATF_CHECK(fmodl(2.0, 0.5) == 0);

	ATF_CHECK(fabsf(fmodf(1.0, 0.1) - 0.1f) <= 55 * FLT_EPSILON);
	ATF_CHECK(fabs(fmod(1.0, 0.1) - 0.1) <= 55 * DBL_EPSILON);
	ATF_CHECK(fabsl(fmodl(1.0, 0.1L) - 0.1L) <= 55 * LDBL_EPSILON);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, fmod);

	return atf_no_error();
}

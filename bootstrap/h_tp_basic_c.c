/*
 * Automated Testing Framework (atf)
 *
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <atf-c.h>

#include "atf-c/error.h"

ATF_TC(pass);
ATF_TC_HEAD(pass, tc)
{
    atf_tc_set_md_var(tc, "descr", "An empty test case that always passes");
}
ATF_TC_BODY(pass, tc)
{
    atf_tc_pass();
}

ATF_TC(fail);
ATF_TC_HEAD(fail, tc)
{
    atf_tc_set_md_var(tc, "descr", "An empty test case that always fails");
}
ATF_TC_BODY(fail, tc)
{
    atf_tc_fail("On purpose");
}

ATF_TC(skip);
ATF_TC_HEAD(skip, tc)
{
    atf_tc_set_md_var(tc, "descr", "An empty test case that is always "
                      "skipped");
}
ATF_TC_BODY(skip, tc)
{
    atf_tc_skip("By design");
}

ATF_TC(default);
ATF_TC_HEAD(default, tc)
{
    atf_tc_set_md_var(tc, "descr", "A test case that passes without "
                      "explicitly stating it");
}
ATF_TC_BODY(default, tc)
{
}

ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, pass);
    ATF_TP_ADD_TC(tp, fail);
    ATF_TP_ADD_TC(tp, skip);
    ATF_TP_ADD_TC(tp, default);

    return atf_no_error();
}

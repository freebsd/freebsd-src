// Copyright (c) 2007 The NetBSD Foundation, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
// CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
// IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <atf-c++.hpp>

ATF_TEST_CASE(pass);

ATF_TEST_CASE_HEAD(pass)
{
    set_md_var("descr", "An empty test case that always passes");
}

ATF_TEST_CASE_BODY(pass)
{
    ATF_PASS();
}

ATF_TEST_CASE(fail);

ATF_TEST_CASE_HEAD(fail)
{
    set_md_var("descr", "An empty test case that always fails");
}

ATF_TEST_CASE_BODY(fail)
{
    ATF_FAIL("On purpose");
}

ATF_TEST_CASE(skip);

ATF_TEST_CASE_HEAD(skip)
{
    set_md_var("descr", "An empty test case that is always skipped");
}

ATF_TEST_CASE_BODY(skip)
{
    ATF_SKIP("By design");
}

ATF_TEST_CASE(default);

ATF_TEST_CASE_HEAD(default)
{
    set_md_var("descr", "A test case that passes without explicitly "
               "stating it");
}

ATF_TEST_CASE_BODY(default)
{
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, pass);
    ATF_ADD_TEST_CASE(tcs, fail);
    ATF_ADD_TEST_CASE(tcs, skip);
    ATF_ADD_TEST_CASE(tcs, default);
}

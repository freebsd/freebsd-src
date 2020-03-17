// Copyright 2011 The Kyua Authors.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <cstdlib>
#include <iostream>

#include <atf-c++.hpp>

#include "utils/test_utils.ipp"


ATF_TEST_CASE_WITHOUT_HEAD(no_properties);
ATF_TEST_CASE_BODY(no_properties)
{
}


ATF_TEST_CASE(one_property);
ATF_TEST_CASE_HEAD(one_property)
{
    set_md_var("descr", "Does nothing but has one metadata property");
}
ATF_TEST_CASE_BODY(one_property)
{
    utils::abort_without_coredump();
}


ATF_TEST_CASE(many_properties);
ATF_TEST_CASE_HEAD(many_properties)
{
    set_md_var("descr", "    A description with some padding");
    set_md_var("require.arch", "some-architecture");
    set_md_var("require.config", "var1 var2 var3");
    set_md_var("require.files", "/my/file1 /some/other/file");
    set_md_var("require.machine", "some-platform");
    set_md_var("require.progs", "bin1 bin2 /nonexistent/bin3");
    set_md_var("require.user", "root");
    set_md_var("X-no-meaning", "I am a custom variable");
}
ATF_TEST_CASE_BODY(many_properties)
{
    utils::abort_without_coredump();
}


ATF_TEST_CASE_WITH_CLEANUP(with_cleanup);
ATF_TEST_CASE_HEAD(with_cleanup)
{
    set_md_var("timeout", "250");
}
ATF_TEST_CASE_BODY(with_cleanup)
{
    std::cout << "Body message to stdout\n";
    std::cerr << "Body message to stderr\n";
}
ATF_TEST_CASE_CLEANUP(with_cleanup)
{
    std::cout << "Cleanup message to stdout\n";
    std::cerr << "Cleanup message to stderr\n";
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, no_properties);
    ATF_ADD_TEST_CASE(tcs, one_property);
    ATF_ADD_TEST_CASE(tcs, many_properties);
    ATF_ADD_TEST_CASE(tcs, with_cleanup);
}

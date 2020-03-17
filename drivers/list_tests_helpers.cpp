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

#include <atf-c++.hpp>

#include "utils/test_utils.ipp"


ATF_TEST_CASE(config_in_head);
ATF_TEST_CASE_HEAD(config_in_head)
{
    if (has_config_var("the-variable")) {
        set_md_var("descr", "the-variable is " +
                   get_config_var("the-variable"));
    }
}
ATF_TEST_CASE_BODY(config_in_head)
{
    utils::abort_without_coredump();
}


ATF_TEST_CASE(crash_list);
ATF_TEST_CASE_HEAD(crash_list)
{
    utils::abort_without_coredump();
}
ATF_TEST_CASE_BODY(crash_list)
{
    utils::abort_without_coredump();
}


ATF_TEST_CASE_WITHOUT_HEAD(no_properties);
ATF_TEST_CASE_BODY(no_properties)
{
    utils::abort_without_coredump();
}


ATF_TEST_CASE(some_properties);
ATF_TEST_CASE_HEAD(some_properties)
{
    set_md_var("descr", "This is a description");
    set_md_var("require.progs", "non-existent /bin/ls");
}
ATF_TEST_CASE_BODY(some_properties)
{
    utils::abort_without_coredump();
}


ATF_INIT_TEST_CASES(tcs)
{
    std::string enabled;

    const char* tests = std::getenv("TESTS");
    if (tests == NULL)
        enabled = "config_in_head crash_list no_properties some_properties";
    else
        enabled = tests;

    if (enabled.find("config_in_head") != std::string::npos)
        ATF_ADD_TEST_CASE(tcs, config_in_head);
    if (enabled.find("crash_list") != std::string::npos)
        ATF_ADD_TEST_CASE(tcs, crash_list);
    if (enabled.find("no_properties") != std::string::npos)
        ATF_ADD_TEST_CASE(tcs, no_properties);
    if (enabled.find("some_properties") != std::string::npos)
        ATF_ADD_TEST_CASE(tcs, some_properties);
}

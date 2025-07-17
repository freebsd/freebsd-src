// Copyright 2010 The Kyua Authors.
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

#include "utils/cmdline/exceptions.hpp"

#include <cstring>

#include <atf-c++.hpp>

namespace cmdline = utils::cmdline;


ATF_TEST_CASE_WITHOUT_HEAD(error);
ATF_TEST_CASE_BODY(error)
{
    const cmdline::error e("Some text");
    ATF_REQUIRE(std::strcmp("Some text", e.what()) == 0);
}


ATF_TEST_CASE_WITHOUT_HEAD(missing_option_argument_error);
ATF_TEST_CASE_BODY(missing_option_argument_error)
{
    const cmdline::missing_option_argument_error e("-o");
    ATF_REQUIRE(std::strcmp("Missing required argument for option -o",
                            e.what()) == 0);
    ATF_REQUIRE_EQ("-o", e.option());
}


ATF_TEST_CASE_WITHOUT_HEAD(option_argument_value_error);
ATF_TEST_CASE_BODY(option_argument_value_error)
{
    const cmdline::option_argument_value_error e("--the_option", "the value",
                                                 "the reason");
    ATF_REQUIRE(std::strcmp("Invalid argument 'the value' for option "
                            "--the_option: the reason", e.what()) == 0);
    ATF_REQUIRE_EQ("--the_option", e.option());
    ATF_REQUIRE_EQ("the value", e.argument());
}


ATF_TEST_CASE_WITHOUT_HEAD(unknown_option_error);
ATF_TEST_CASE_BODY(unknown_option_error)
{
    const cmdline::unknown_option_error e("--foo");
    ATF_REQUIRE(std::strcmp("Unknown option --foo", e.what()) == 0);
    ATF_REQUIRE_EQ("--foo", e.option());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, error);
    ATF_ADD_TEST_CASE(tcs, missing_option_argument_error);
    ATF_ADD_TEST_CASE(tcs, option_argument_value_error);
    ATF_ADD_TEST_CASE(tcs, unknown_option_error);
}

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

#include "utils/format/exceptions.hpp"

#include <cstring>

#include <atf-c++.hpp>

using utils::format::bad_format_error;
using utils::format::error;
using utils::format::extra_args_error;


ATF_TEST_CASE_WITHOUT_HEAD(error);
ATF_TEST_CASE_BODY(error)
{
    const error e("Some text");
    ATF_REQUIRE(std::strcmp("Some text", e.what()) == 0);
}


ATF_TEST_CASE_WITHOUT_HEAD(bad_format_error);
ATF_TEST_CASE_BODY(bad_format_error)
{
    const bad_format_error e("format-string", "the-error");
    ATF_REQUIRE(std::strcmp("Invalid formatting string 'format-string': "
                            "the-error", e.what()) == 0);
    ATF_REQUIRE_EQ("format-string", e.format());
}


ATF_TEST_CASE_WITHOUT_HEAD(extra_args_error);
ATF_TEST_CASE_BODY(extra_args_error)
{
    const extra_args_error e("fmt", "extra");
    ATF_REQUIRE(std::strcmp("Not enough fields in formatting string 'fmt' to "
                            "place argument 'extra'", e.what()) == 0);
    ATF_REQUIRE_EQ("fmt", e.format());
    ATF_REQUIRE_EQ("extra", e.arg());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, error);
    ATF_ADD_TEST_CASE(tcs, bad_format_error);
    ATF_ADD_TEST_CASE(tcs, extra_args_error);
}

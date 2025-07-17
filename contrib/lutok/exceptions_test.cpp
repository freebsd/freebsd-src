// Copyright 2011 Google Inc.
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

#include "exceptions.hpp"

#include <cstring>

#include <atf-c++.hpp>

#include "state.ipp"


ATF_TEST_CASE_WITHOUT_HEAD(error);
ATF_TEST_CASE_BODY(error)
{
    const lutok::error e("Some text");
    ATF_REQUIRE(std::strcmp("Some text", e.what()) == 0);
}


ATF_TEST_CASE_WITHOUT_HEAD(api_error__explicit);
ATF_TEST_CASE_BODY(api_error__explicit)
{
    const lutok::api_error e("some_function", "Some text");
    ATF_REQUIRE(std::strcmp("Some text", e.what()) == 0);
    ATF_REQUIRE_EQ("some_function", e.api_function());
}


ATF_TEST_CASE_WITHOUT_HEAD(api_error__from_stack);
ATF_TEST_CASE_BODY(api_error__from_stack)
{
    lutok::state state;
    state.push_integer(123);
    state.push_string("The error message");
    const lutok::api_error e = lutok::api_error::from_stack(state,
                                                            "the_function");
    ATF_REQUIRE_EQ(1, state.get_top());
    ATF_REQUIRE_EQ(123, state.to_integer(-1));
    state.pop(1);
    ATF_REQUIRE(std::strcmp("The error message", e.what()) == 0);
    ATF_REQUIRE_EQ("the_function", e.api_function());
}


ATF_TEST_CASE_WITHOUT_HEAD(file_not_found_error);
ATF_TEST_CASE_BODY(file_not_found_error)
{
    const lutok::file_not_found_error e("missing-file");
    ATF_REQUIRE(std::strcmp("File 'missing-file' not found", e.what()) == 0);
    ATF_REQUIRE_EQ("missing-file", e.filename());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, error);

    ATF_ADD_TEST_CASE(tcs, api_error__explicit);
    ATF_ADD_TEST_CASE(tcs, api_error__from_stack);

    ATF_ADD_TEST_CASE(tcs, file_not_found_error);
}

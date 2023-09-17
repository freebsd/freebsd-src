// Copyright 2012 The Kyua Authors.
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

#include "utils/config/exceptions.hpp"

#include <cstring>

#include <atf-c++.hpp>

#include "utils/config/tree.ipp"

namespace config = utils::config;
namespace detail = utils::config::detail;


ATF_TEST_CASE_WITHOUT_HEAD(error);
ATF_TEST_CASE_BODY(error)
{
    const config::error e("Some text");
    ATF_REQUIRE(std::strcmp("Some text", e.what()) == 0);
}


ATF_TEST_CASE_WITHOUT_HEAD(bad_combination_error);
ATF_TEST_CASE_BODY(bad_combination_error)
{
    detail::tree_key key;
    key.push_back("first");
    key.push_back("second");

    const config::bad_combination_error e(key, "Failed to combine '%s'");
    ATF_REQUIRE(std::strcmp("Failed to combine 'first.second'", e.what()) == 0);
}


ATF_TEST_CASE_WITHOUT_HEAD(invalid_key_error);
ATF_TEST_CASE_BODY(invalid_key_error)
{
    const config::invalid_key_error e("Some text");
    ATF_REQUIRE(std::strcmp("Some text", e.what()) == 0);
}


ATF_TEST_CASE_WITHOUT_HEAD(invalid_key_value);
ATF_TEST_CASE_BODY(invalid_key_value)
{
    detail::tree_key key;
    key.push_back("1");
    key.push_back("two");

    const config::invalid_key_value e(key, "foo bar");
    ATF_REQUIRE(std::strcmp("Invalid value for property '1.two': foo bar",
                            e.what()) == 0);
}


ATF_TEST_CASE_WITHOUT_HEAD(syntax_error);
ATF_TEST_CASE_BODY(syntax_error)
{
    const config::syntax_error e("Some text");
    ATF_REQUIRE(std::strcmp("Some text", e.what()) == 0);
}


ATF_TEST_CASE_WITHOUT_HEAD(unknown_key_error__default_message);
ATF_TEST_CASE_BODY(unknown_key_error__default_message)
{
    detail::tree_key key;
    key.push_back("1");
    key.push_back("two");

    const config::unknown_key_error e(key);
    ATF_REQUIRE(std::strcmp("Unknown configuration property '1.two'",
                            e.what()) == 0);
}


ATF_TEST_CASE_WITHOUT_HEAD(unknown_key_error__custom_message);
ATF_TEST_CASE_BODY(unknown_key_error__custom_message)
{
    detail::tree_key key;
    key.push_back("1");
    key.push_back("two");

    const config::unknown_key_error e(key, "The test '%s' string");
    ATF_REQUIRE(std::strcmp("The test '1.two' string", e.what()) == 0);
}


ATF_TEST_CASE_WITHOUT_HEAD(value_error);
ATF_TEST_CASE_BODY(value_error)
{
    const config::value_error e("Some text");
    ATF_REQUIRE(std::strcmp("Some text", e.what()) == 0);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, error);
    ATF_ADD_TEST_CASE(tcs, bad_combination_error);
    ATF_ADD_TEST_CASE(tcs, invalid_key_error);
    ATF_ADD_TEST_CASE(tcs, invalid_key_value);
    ATF_ADD_TEST_CASE(tcs, syntax_error);
    ATF_ADD_TEST_CASE(tcs, unknown_key_error__default_message);
    ATF_ADD_TEST_CASE(tcs, unknown_key_error__custom_message);
    ATF_ADD_TEST_CASE(tcs, value_error);
}

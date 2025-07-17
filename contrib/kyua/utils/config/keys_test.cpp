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

#include "utils/config/keys.hpp"

#include <atf-c++.hpp>

#include "utils/config/exceptions.hpp"

namespace config = utils::config;


ATF_TEST_CASE_WITHOUT_HEAD(flatten_key__one);
ATF_TEST_CASE_BODY(flatten_key__one)
{
    config::detail::tree_key key;
    key.push_back("foo");
    ATF_REQUIRE_EQ("foo", config::detail::flatten_key(key));
}


ATF_TEST_CASE_WITHOUT_HEAD(flatten_key__many);
ATF_TEST_CASE_BODY(flatten_key__many)
{
    config::detail::tree_key key;
    key.push_back("foo");
    key.push_back("1");
    key.push_back("bar");
    ATF_REQUIRE_EQ("foo.1.bar", config::detail::flatten_key(key));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_key__one);
ATF_TEST_CASE_BODY(parse_key__one)
{
    config::detail::tree_key exp_key;
    exp_key.push_back("one");
    ATF_REQUIRE(exp_key == config::detail::parse_key("one"));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_key__many);
ATF_TEST_CASE_BODY(parse_key__many)
{
    config::detail::tree_key exp_key;
    exp_key.push_back("one");
    exp_key.push_back("2");
    exp_key.push_back("foo");
    ATF_REQUIRE(exp_key == config::detail::parse_key("one.2.foo"));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_key__empty_key);
ATF_TEST_CASE_BODY(parse_key__empty_key)
{
    ATF_REQUIRE_THROW_RE(config::invalid_key_error,
                         "Empty key",
                         config::detail::parse_key(""));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_key__empty_component);
ATF_TEST_CASE_BODY(parse_key__empty_component)
{
    ATF_REQUIRE_THROW_RE(config::invalid_key_error,
                         "Empty component in key '.'",
                         config::detail::parse_key("."));
    ATF_REQUIRE_THROW_RE(config::invalid_key_error,
                         "Empty component in key 'a.'",
                         config::detail::parse_key("a."));
    ATF_REQUIRE_THROW_RE(config::invalid_key_error,
                         "Empty component in key '.b'",
                         config::detail::parse_key(".b"));
    ATF_REQUIRE_THROW_RE(config::invalid_key_error,
                         "Empty component in key 'a..b'",
                         config::detail::parse_key("a..b"));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, flatten_key__one);
    ATF_ADD_TEST_CASE(tcs, flatten_key__many);

    ATF_ADD_TEST_CASE(tcs, parse_key__one);
    ATF_ADD_TEST_CASE(tcs, parse_key__many);
    ATF_ADD_TEST_CASE(tcs, parse_key__empty_key);
    ATF_ADD_TEST_CASE(tcs, parse_key__empty_component);
}

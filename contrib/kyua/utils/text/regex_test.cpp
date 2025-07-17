// Copyright 2014 The Kyua Authors.
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

#include "utils/text/regex.hpp"

#include <atf-c++.hpp>

#include "utils/text/exceptions.hpp"

namespace text = utils::text;


ATF_TEST_CASE_WITHOUT_HEAD(integration__no_matches);
ATF_TEST_CASE_BODY(integration__no_matches)
{
    const text::regex_matches matches = text::match_regex(
        "foo.*bar", "this is a string without the searched text", 0);
    ATF_REQUIRE(!matches);
    ATF_REQUIRE_EQ(0, matches.count());
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__no_capture_groups);
ATF_TEST_CASE_BODY(integration__no_capture_groups)
{
    const text::regex_matches matches = text::match_regex(
        "foo.*bar", "this is a string with foo and bar embedded in it", 0);
    ATF_REQUIRE(matches);
    ATF_REQUIRE_EQ(1, matches.count());
    ATF_REQUIRE_EQ("foo and bar", matches.get(0));
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__one_capture_group);
ATF_TEST_CASE_BODY(integration__one_capture_group)
{
    const text::regex_matches matches = text::match_regex(
        "^([^ ]*) ", "the string", 1);
    ATF_REQUIRE(matches);
    ATF_REQUIRE_EQ(2, matches.count());
    ATF_REQUIRE_EQ("the ", matches.get(0));
    ATF_REQUIRE_EQ("the", matches.get(1));
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__many_capture_groups);
ATF_TEST_CASE_BODY(integration__many_capture_groups)
{
    const text::regex_matches matches = text::match_regex(
        "is ([^ ]*) ([a-z]*) to", "this is another string to parse", 2);
    ATF_REQUIRE(matches);
    ATF_REQUIRE_EQ(3, matches.count());
    ATF_REQUIRE_EQ("is another string to", matches.get(0));
    ATF_REQUIRE_EQ("another", matches.get(1));
    ATF_REQUIRE_EQ("string", matches.get(2));
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__capture_groups_underspecified);
ATF_TEST_CASE_BODY(integration__capture_groups_underspecified)
{
    const text::regex_matches matches = text::match_regex(
        "is ([^ ]*) ([a-z]*) to", "this is another string to parse", 1);
    ATF_REQUIRE(matches);
    ATF_REQUIRE_EQ(2, matches.count());
    ATF_REQUIRE_EQ("is another string to", matches.get(0));
    ATF_REQUIRE_EQ("another", matches.get(1));
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__capture_groups_overspecified);
ATF_TEST_CASE_BODY(integration__capture_groups_overspecified)
{
    const text::regex_matches matches = text::match_regex(
        "is ([^ ]*) ([a-z]*) to", "this is another string to parse", 10);
    ATF_REQUIRE(matches);
    ATF_REQUIRE_EQ(3, matches.count());
    ATF_REQUIRE_EQ("is another string to", matches.get(0));
    ATF_REQUIRE_EQ("another", matches.get(1));
    ATF_REQUIRE_EQ("string", matches.get(2));
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__reuse_regex_in_multiple_matches);
ATF_TEST_CASE_BODY(integration__reuse_regex_in_multiple_matches)
{
    const text::regex regex = text::regex::compile("number is ([0-9]+)", 1);

    {
        const text::regex_matches matches = regex.match("my number is 581.");
        ATF_REQUIRE(matches);
        ATF_REQUIRE_EQ(2, matches.count());
        ATF_REQUIRE_EQ("number is 581", matches.get(0));
        ATF_REQUIRE_EQ("581", matches.get(1));
    }

    {
        const text::regex_matches matches = regex.match("your number is 6");
        ATF_REQUIRE(matches);
        ATF_REQUIRE_EQ(2, matches.count());
        ATF_REQUIRE_EQ("number is 6", matches.get(0));
        ATF_REQUIRE_EQ("6", matches.get(1));
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__ignore_case);
ATF_TEST_CASE_BODY(integration__ignore_case)
{
    const text::regex regex1 = text::regex::compile("foo", 0, false);
    ATF_REQUIRE(!regex1.match("bar Foo bar"));
    ATF_REQUIRE(!regex1.match("bar foO bar"));
    ATF_REQUIRE(!regex1.match("bar FOO bar"));

    ATF_REQUIRE(!text::match_regex("foo", "bar Foo bar", 0, false));
    ATF_REQUIRE(!text::match_regex("foo", "bar foO bar", 0, false));
    ATF_REQUIRE(!text::match_regex("foo", "bar FOO bar", 0, false));

    const text::regex regex2 = text::regex::compile("foo", 0, true);
    ATF_REQUIRE( regex2.match("bar foo bar"));
    ATF_REQUIRE( regex2.match("bar Foo bar"));
    ATF_REQUIRE( regex2.match("bar foO bar"));
    ATF_REQUIRE( regex2.match("bar FOO bar"));

    ATF_REQUIRE( text::match_regex("foo", "bar foo bar", 0, true));
    ATF_REQUIRE( text::match_regex("foo", "bar Foo bar", 0, true));
    ATF_REQUIRE( text::match_regex("foo", "bar foO bar", 0, true));
    ATF_REQUIRE( text::match_regex("foo", "bar FOO bar", 0, true));
}

ATF_TEST_CASE_WITHOUT_HEAD(integration__invalid_regex);
ATF_TEST_CASE_BODY(integration__invalid_regex)
{
    ATF_REQUIRE_THROW(text::regex_error,
                      text::regex::compile("this is (unbalanced", 0));
}


ATF_INIT_TEST_CASES(tcs)
{
    // regex and regex_matches are so coupled that it makes no sense to test
    // them independently.  Just validate their integration.
    ATF_ADD_TEST_CASE(tcs, integration__no_matches);
    ATF_ADD_TEST_CASE(tcs, integration__no_capture_groups);
    ATF_ADD_TEST_CASE(tcs, integration__one_capture_group);
    ATF_ADD_TEST_CASE(tcs, integration__many_capture_groups);
    ATF_ADD_TEST_CASE(tcs, integration__capture_groups_underspecified);
    ATF_ADD_TEST_CASE(tcs, integration__capture_groups_overspecified);
    ATF_ADD_TEST_CASE(tcs, integration__reuse_regex_in_multiple_matches);
    ATF_ADD_TEST_CASE(tcs, integration__ignore_case);
    ATF_ADD_TEST_CASE(tcs, integration__invalid_regex);
}

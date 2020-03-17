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

#include "engine/filters.hpp"

#include <stdexcept>

#include <atf-c++.hpp>

namespace fs = utils::fs;


namespace {


/// Syntactic sugar to instantiate engine::test_filter objects.
///
/// \param test_program Test program.
/// \param test_case Test case.
///
/// \return A \p test_filter object, based on \p test_program and \p test_case.
inline engine::test_filter
mkfilter(const char* test_program, const char* test_case)
{
    return engine::test_filter(fs::path(test_program), test_case);
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__public_fields);
ATF_TEST_CASE_BODY(test_filter__public_fields)
{
    const engine::test_filter filter(fs::path("foo/bar"), "baz");
    ATF_REQUIRE_EQ(fs::path("foo/bar"), filter.test_program);
    ATF_REQUIRE_EQ("baz", filter.test_case);
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__parse__ok);
ATF_TEST_CASE_BODY(test_filter__parse__ok)
{
    const engine::test_filter filter(engine::test_filter::parse("foo"));
    ATF_REQUIRE_EQ(fs::path("foo"), filter.test_program);
    ATF_REQUIRE(filter.test_case.empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__parse__empty);
ATF_TEST_CASE_BODY(test_filter__parse__empty)
{
    ATF_REQUIRE_THROW_RE(std::runtime_error, "empty",
                         engine::test_filter::parse(""));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__parse__absolute);
ATF_TEST_CASE_BODY(test_filter__parse__absolute)
{
    ATF_REQUIRE_THROW_RE(std::runtime_error, "'/foo/bar'.*relative",
                         engine::test_filter::parse("/foo//bar"));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__parse__bad_program_name);
ATF_TEST_CASE_BODY(test_filter__parse__bad_program_name)
{
    ATF_REQUIRE_THROW_RE(std::runtime_error, "Program name.*':foo'",
                         engine::test_filter::parse(":foo"));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__parse__bad_test_case);
ATF_TEST_CASE_BODY(test_filter__parse__bad_test_case)
{
    ATF_REQUIRE_THROW_RE(std::runtime_error, "Test case.*'bar/baz:'",
                         engine::test_filter::parse("bar/baz:"));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__parse__bad_path);
ATF_TEST_CASE_BODY(test_filter__parse__bad_path)
{
    // TODO(jmmv): Not implemented.  At the moment, the only reason for a path
    // to be invalid is if it is empty... but we are checking this exact
    // condition ourselves as part of the input validation.  So we can't mock in
    // an argument with an invalid non-empty path...
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__str);
ATF_TEST_CASE_BODY(test_filter__str)
{
    const engine::test_filter filter(fs::path("foo/bar"), "baz");
    ATF_REQUIRE_EQ("foo/bar:baz", filter.str());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__contains__same);
ATF_TEST_CASE_BODY(test_filter__contains__same)
{
    {
        const engine::test_filter f(fs::path("foo/bar"), "baz");
        ATF_REQUIRE(f.contains(f));
    }
    {
        const engine::test_filter f(fs::path("foo/bar"), "");
        ATF_REQUIRE(f.contains(f));
    }
    {
        const engine::test_filter f(fs::path("foo"), "");
        ATF_REQUIRE(f.contains(f));
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__contains__different);
ATF_TEST_CASE_BODY(test_filter__contains__different)
{
    {
        const engine::test_filter f1(fs::path("foo"), "");
        const engine::test_filter f2(fs::path("foo"), "bar");
        ATF_REQUIRE( f1.contains(f2));
        ATF_REQUIRE(!f2.contains(f1));
    }
    {
        const engine::test_filter f1(fs::path("foo/bar"), "");
        const engine::test_filter f2(fs::path("foo/bar"), "baz");
        ATF_REQUIRE( f1.contains(f2));
        ATF_REQUIRE(!f2.contains(f1));
    }
    {
        const engine::test_filter f1(fs::path("foo/bar"), "");
        const engine::test_filter f2(fs::path("foo/baz"), "");
        ATF_REQUIRE(!f1.contains(f2));
        ATF_REQUIRE(!f2.contains(f1));
    }
    {
        const engine::test_filter f1(fs::path("foo"), "");
        const engine::test_filter f2(fs::path("foo/bar"), "");
        ATF_REQUIRE( f1.contains(f2));
        ATF_REQUIRE(!f2.contains(f1));
    }
    {
        const engine::test_filter f1(fs::path("foo"), "bar");
        const engine::test_filter f2(fs::path("foo/bar"), "");
        ATF_REQUIRE(!f1.contains(f2));
        ATF_REQUIRE(!f2.contains(f1));
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__matches_test_program)
ATF_TEST_CASE_BODY(test_filter__matches_test_program)
{
    {
        const engine::test_filter f(fs::path("top"), "unused");
        ATF_REQUIRE( f.matches_test_program(fs::path("top")));
        ATF_REQUIRE(!f.matches_test_program(fs::path("top2")));
    }

    {
        const engine::test_filter f(fs::path("dir1/dir2"), "");
        ATF_REQUIRE( f.matches_test_program(fs::path("dir1/dir2/foo")));
        ATF_REQUIRE( f.matches_test_program(fs::path("dir1/dir2/bar")));
        ATF_REQUIRE( f.matches_test_program(fs::path("dir1/dir2/bar/baz")));
        ATF_REQUIRE( f.matches_test_program(fs::path("dir1/dir2/bar/baz")));
        ATF_REQUIRE(!f.matches_test_program(fs::path("dir1")));
        ATF_REQUIRE(!f.matches_test_program(fs::path("dir1/bar/baz")));
        ATF_REQUIRE(!f.matches_test_program(fs::path("dir2/bar/baz")));
    }

    {
        const engine::test_filter f(fs::path("dir1/dir2"), "unused");
        ATF_REQUIRE( f.matches_test_program(fs::path("dir1/dir2")));
        ATF_REQUIRE(!f.matches_test_program(fs::path("dir1/dir2/foo")));
        ATF_REQUIRE(!f.matches_test_program(fs::path("dir1/dir2/bar")));
        ATF_REQUIRE(!f.matches_test_program(fs::path("dir1/dir2/bar/baz")));
        ATF_REQUIRE(!f.matches_test_program(fs::path("dir1/dir2/bar/baz")));
        ATF_REQUIRE(!f.matches_test_program(fs::path("dir1")));
        ATF_REQUIRE(!f.matches_test_program(fs::path("dir1/bar/baz")));
        ATF_REQUIRE(!f.matches_test_program(fs::path("dir2/bar/baz")));
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__matches_test_case)
ATF_TEST_CASE_BODY(test_filter__matches_test_case)
{
    {
        const engine::test_filter f(fs::path("top"), "foo");
        ATF_REQUIRE( f.matches_test_case(fs::path("top"), "foo"));
        ATF_REQUIRE(!f.matches_test_case(fs::path("top"), "bar"));
    }

    {
        const engine::test_filter f(fs::path("top"), "");
        ATF_REQUIRE( f.matches_test_case(fs::path("top"), "foo"));
        ATF_REQUIRE( f.matches_test_case(fs::path("top"), "bar"));
        ATF_REQUIRE(!f.matches_test_case(fs::path("top2"), "foo"));
    }

    {
        const engine::test_filter f(fs::path("d1/d2/prog"), "t1");
        ATF_REQUIRE( f.matches_test_case(fs::path("d1/d2/prog"), "t1"));
        ATF_REQUIRE(!f.matches_test_case(fs::path("d1/d2/prog"), "t2"));
    }

    {
        const engine::test_filter f(fs::path("d1/d2"), "");
        ATF_REQUIRE( f.matches_test_case(fs::path("d1/d2/prog"), "t1"));
        ATF_REQUIRE( f.matches_test_case(fs::path("d1/d2/prog"), "t2"));
        ATF_REQUIRE( f.matches_test_case(fs::path("d1/d2/prog2"), "t2"));
        ATF_REQUIRE(!f.matches_test_case(fs::path("d1/d3"), "foo"));
        ATF_REQUIRE(!f.matches_test_case(fs::path("d2"), "foo"));
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__operator_lt)
ATF_TEST_CASE_BODY(test_filter__operator_lt)
{
    {
        const engine::test_filter f1(fs::path("d1/d2"), "");
        ATF_REQUIRE(!(f1 < f1));
    }
    {
        const engine::test_filter f1(fs::path("d1/d2"), "");
        const engine::test_filter f2(fs::path("d1/d3"), "");
        ATF_REQUIRE( (f1 < f2));
        ATF_REQUIRE(!(f2 < f1));
    }
    {
        const engine::test_filter f1(fs::path("d1/d2"), "");
        const engine::test_filter f2(fs::path("d1/d2"), "foo");
        ATF_REQUIRE( (f1 < f2));
        ATF_REQUIRE(!(f2 < f1));
    }
    {
        const engine::test_filter f1(fs::path("d1/d2"), "bar");
        const engine::test_filter f2(fs::path("d1/d2"), "foo");
        ATF_REQUIRE( (f1 < f2));
        ATF_REQUIRE(!(f2 < f1));
    }
    {
        const engine::test_filter f1(fs::path("d1/d2"), "bar");
        const engine::test_filter f2(fs::path("d1/d3"), "");
        ATF_REQUIRE( (f1 < f2));
        ATF_REQUIRE(!(f2 < f1));
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__operator_eq)
ATF_TEST_CASE_BODY(test_filter__operator_eq)
{
    const engine::test_filter f1(fs::path("d1/d2"), "");
    const engine::test_filter f2(fs::path("d1/d2"), "bar");
    ATF_REQUIRE( (f1 == f1));
    ATF_REQUIRE(!(f1 == f2));
    ATF_REQUIRE(!(f2 == f1));
    ATF_REQUIRE( (f2 == f2));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__operator_ne)
ATF_TEST_CASE_BODY(test_filter__operator_ne)
{
    const engine::test_filter f1(fs::path("d1/d2"), "");
    const engine::test_filter f2(fs::path("d1/d2"), "bar");
    ATF_REQUIRE(!(f1 != f1));
    ATF_REQUIRE( (f1 != f2));
    ATF_REQUIRE( (f2 != f1));
    ATF_REQUIRE(!(f2 != f2));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filter__output);
ATF_TEST_CASE_BODY(test_filter__output)
{
    {
        std::ostringstream str;
        str << engine::test_filter(fs::path("d1/d2"), "");
        ATF_REQUIRE_EQ(
            "test_filter{test_program=d1/d2}",
            str.str());
    }
    {
        std::ostringstream str;
        str << engine::test_filter(fs::path("d1/d2"), "bar");
        ATF_REQUIRE_EQ(
            "test_filter{test_program=d1/d2, test_case=bar}",
            str.str());
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__match_test_case__no_filters)
ATF_TEST_CASE_BODY(test_filters__match_test_case__no_filters)
{
    const std::set< engine::test_filter > raw_filters;

    const engine::test_filters filters(raw_filters);
    engine::test_filters::match match;

    match = filters.match_test_case(fs::path("foo"), "baz");
    ATF_REQUIRE(match.first);
    ATF_REQUIRE(!match.second);

    match = filters.match_test_case(fs::path("foo/bar"), "baz");
    ATF_REQUIRE(match.first);
    ATF_REQUIRE(!match.second);
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__match_test_case__some_filters)
ATF_TEST_CASE_BODY(test_filters__match_test_case__some_filters)
{
    std::set< engine::test_filter > raw_filters;
    raw_filters.insert(mkfilter("top_test", ""));
    raw_filters.insert(mkfilter("subdir_1", ""));
    raw_filters.insert(mkfilter("subdir_2/a_test", ""));
    raw_filters.insert(mkfilter("subdir_2/b_test", "foo"));

    const engine::test_filters filters(raw_filters);
    engine::test_filters::match match;

    match = filters.match_test_case(fs::path("top_test"), "a");
    ATF_REQUIRE(match.first);
    ATF_REQUIRE_EQ("top_test", match.second.get().str());

    match = filters.match_test_case(fs::path("subdir_1/foo"), "a");
    ATF_REQUIRE(match.first);
    ATF_REQUIRE_EQ("subdir_1", match.second.get().str());

    match = filters.match_test_case(fs::path("subdir_1/bar"), "z");
    ATF_REQUIRE(match.first);
    ATF_REQUIRE_EQ("subdir_1", match.second.get().str());

    match = filters.match_test_case(fs::path("subdir_2/a_test"), "bar");
    ATF_REQUIRE(match.first);
    ATF_REQUIRE_EQ("subdir_2/a_test", match.second.get().str());

    match = filters.match_test_case(fs::path("subdir_2/b_test"), "foo");
    ATF_REQUIRE(match.first);
    ATF_REQUIRE_EQ("subdir_2/b_test:foo", match.second.get().str());

    match = filters.match_test_case(fs::path("subdir_2/b_test"), "bar");
    ATF_REQUIRE(!match.first);

    match = filters.match_test_case(fs::path("subdir_2/c_test"), "foo");
    ATF_REQUIRE(!match.first);

    match = filters.match_test_case(fs::path("subdir_3"), "hello");
    ATF_REQUIRE(!match.first);
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__match_test_program__no_filters)
ATF_TEST_CASE_BODY(test_filters__match_test_program__no_filters)
{
    const std::set< engine::test_filter > raw_filters;

    const engine::test_filters filters(raw_filters);
    ATF_REQUIRE(filters.match_test_program(fs::path("foo")));
    ATF_REQUIRE(filters.match_test_program(fs::path("foo/bar")));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__match_test_program__some_filters)
ATF_TEST_CASE_BODY(test_filters__match_test_program__some_filters)
{
    std::set< engine::test_filter > raw_filters;
    raw_filters.insert(mkfilter("top_test", ""));
    raw_filters.insert(mkfilter("subdir_1", ""));
    raw_filters.insert(mkfilter("subdir_2/a_test", ""));
    raw_filters.insert(mkfilter("subdir_2/b_test", "foo"));

    const engine::test_filters filters(raw_filters);
    ATF_REQUIRE( filters.match_test_program(fs::path("top_test")));
    ATF_REQUIRE( filters.match_test_program(fs::path("subdir_1/foo")));
    ATF_REQUIRE( filters.match_test_program(fs::path("subdir_1/bar")));
    ATF_REQUIRE( filters.match_test_program(fs::path("subdir_2/a_test")));
    ATF_REQUIRE( filters.match_test_program(fs::path("subdir_2/b_test")));
    ATF_REQUIRE(!filters.match_test_program(fs::path("subdir_2/c_test")));
    ATF_REQUIRE(!filters.match_test_program(fs::path("subdir_3")));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__difference__no_filters);
ATF_TEST_CASE_BODY(test_filters__difference__no_filters)
{
    const std::set< engine::test_filter > in_filters;
    const std::set< engine::test_filter > used;
    const std::set< engine::test_filter > diff = engine::test_filters(
        in_filters).difference(used);
    ATF_REQUIRE(diff.empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__difference__some_filters__all_used);
ATF_TEST_CASE_BODY(test_filters__difference__some_filters__all_used)
{
    std::set< engine::test_filter > in_filters;
    in_filters.insert(mkfilter("a", ""));
    in_filters.insert(mkfilter("b", "c"));

    const std::set< engine::test_filter > used = in_filters;

    const std::set< engine::test_filter > diff = engine::test_filters(
        in_filters).difference(used);
    ATF_REQUIRE(diff.empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_filters__difference__some_filters__some_unused);
ATF_TEST_CASE_BODY(test_filters__difference__some_filters__some_unused)
{
    std::set< engine::test_filter > in_filters;
    in_filters.insert(mkfilter("a", ""));
    in_filters.insert(mkfilter("b", "c"));
    in_filters.insert(mkfilter("d", ""));
    in_filters.insert(mkfilter("e", "f"));

    std::set< engine::test_filter > used;
    used.insert(mkfilter("b", "c"));
    used.insert(mkfilter("d", ""));

    const std::set< engine::test_filter > diff = engine::test_filters(
        in_filters).difference(used);
    ATF_REQUIRE_EQ(2, diff.size());
    ATF_REQUIRE(diff.find(mkfilter("a", "")) != diff.end());
    ATF_REQUIRE(diff.find(mkfilter("e", "f")) != diff.end());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_disjoint_filters__ok);
ATF_TEST_CASE_BODY(check_disjoint_filters__ok)
{
    std::set< engine::test_filter > filters;
    filters.insert(mkfilter("a", ""));
    filters.insert(mkfilter("b", ""));
    filters.insert(mkfilter("c", "a"));
    filters.insert(mkfilter("c", "b"));

    engine::check_disjoint_filters(filters);
}


ATF_TEST_CASE_WITHOUT_HEAD(check_disjoint_filters__fail);
ATF_TEST_CASE_BODY(check_disjoint_filters__fail)
{
    std::set< engine::test_filter > filters;
    filters.insert(mkfilter("a", ""));
    filters.insert(mkfilter("b", ""));
    filters.insert(mkfilter("c", "a"));
    filters.insert(mkfilter("d", "b"));
    filters.insert(mkfilter("c", ""));

    ATF_REQUIRE_THROW_RE(std::runtime_error, "'c'.*'c:a'.*not disjoint",
                         engine::check_disjoint_filters(filters));
}


ATF_TEST_CASE_WITHOUT_HEAD(filters_state__match_test_program);
ATF_TEST_CASE_BODY(filters_state__match_test_program)
{
    std::set< engine::test_filter > filters;
    filters.insert(mkfilter("foo/bar", ""));
    filters.insert(mkfilter("baz", "tc"));
    engine::filters_state state(filters);

    ATF_REQUIRE(state.match_test_program(fs::path("foo/bar/something")));
    ATF_REQUIRE(state.match_test_program(fs::path("baz")));

    ATF_REQUIRE(!state.match_test_program(fs::path("foo/baz")));
    ATF_REQUIRE(!state.match_test_program(fs::path("hello")));
}


ATF_TEST_CASE_WITHOUT_HEAD(filters_state__match_test_case);
ATF_TEST_CASE_BODY(filters_state__match_test_case)
{
    std::set< engine::test_filter > filters;
    filters.insert(mkfilter("foo/bar", ""));
    filters.insert(mkfilter("baz", "tc"));
    engine::filters_state state(filters);

    ATF_REQUIRE(state.match_test_case(fs::path("foo/bar/something"), "any"));
    ATF_REQUIRE(state.match_test_case(fs::path("baz"), "tc"));

    ATF_REQUIRE(!state.match_test_case(fs::path("foo/baz/something"), "tc"));
    ATF_REQUIRE(!state.match_test_case(fs::path("baz"), "tc2"));
}


ATF_TEST_CASE_WITHOUT_HEAD(filters_state__unused__none);
ATF_TEST_CASE_BODY(filters_state__unused__none)
{
    std::set< engine::test_filter > filters;
    filters.insert(mkfilter("a/b", ""));
    filters.insert(mkfilter("baz", "tc"));
    filters.insert(mkfilter("hey/d", "yes"));
    engine::filters_state state(filters);

    state.match_test_case(fs::path("a/b/c"), "any");
    state.match_test_case(fs::path("baz"), "tc");
    state.match_test_case(fs::path("hey/d"), "yes");

    ATF_REQUIRE(state.unused().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(filters_state__unused__some);
ATF_TEST_CASE_BODY(filters_state__unused__some)
{
    std::set< engine::test_filter > filters;
    filters.insert(mkfilter("a/b", ""));
    filters.insert(mkfilter("baz", "tc"));
    filters.insert(mkfilter("hey/d", "yes"));
    engine::filters_state state(filters);

    state.match_test_program(fs::path("a/b/c"));
    state.match_test_case(fs::path("baz"), "tc");

    std::set< engine::test_filter > exp_unused;
    exp_unused.insert(mkfilter("a/b", ""));
    exp_unused.insert(mkfilter("hey/d", "yes"));

    ATF_REQUIRE(exp_unused == state.unused());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, test_filter__public_fields);
    ATF_ADD_TEST_CASE(tcs, test_filter__parse__ok);
    ATF_ADD_TEST_CASE(tcs, test_filter__parse__empty);
    ATF_ADD_TEST_CASE(tcs, test_filter__parse__absolute);
    ATF_ADD_TEST_CASE(tcs, test_filter__parse__bad_program_name);
    ATF_ADD_TEST_CASE(tcs, test_filter__parse__bad_test_case);
    ATF_ADD_TEST_CASE(tcs, test_filter__parse__bad_path);
    ATF_ADD_TEST_CASE(tcs, test_filter__str);
    ATF_ADD_TEST_CASE(tcs, test_filter__contains__same);
    ATF_ADD_TEST_CASE(tcs, test_filter__contains__different);
    ATF_ADD_TEST_CASE(tcs, test_filter__matches_test_program);
    ATF_ADD_TEST_CASE(tcs, test_filter__matches_test_case);
    ATF_ADD_TEST_CASE(tcs, test_filter__operator_lt);
    ATF_ADD_TEST_CASE(tcs, test_filter__operator_eq);
    ATF_ADD_TEST_CASE(tcs, test_filter__operator_ne);
    ATF_ADD_TEST_CASE(tcs, test_filter__output);

    ATF_ADD_TEST_CASE(tcs, test_filters__match_test_case__no_filters);
    ATF_ADD_TEST_CASE(tcs, test_filters__match_test_case__some_filters);
    ATF_ADD_TEST_CASE(tcs, test_filters__match_test_program__no_filters);
    ATF_ADD_TEST_CASE(tcs, test_filters__match_test_program__some_filters);
    ATF_ADD_TEST_CASE(tcs, test_filters__difference__no_filters);
    ATF_ADD_TEST_CASE(tcs, test_filters__difference__some_filters__all_used);
    ATF_ADD_TEST_CASE(tcs, test_filters__difference__some_filters__some_unused);

    ATF_ADD_TEST_CASE(tcs, check_disjoint_filters__ok);
    ATF_ADD_TEST_CASE(tcs, check_disjoint_filters__fail);

    ATF_ADD_TEST_CASE(tcs, filters_state__match_test_program);
    ATF_ADD_TEST_CASE(tcs, filters_state__match_test_case);
    ATF_ADD_TEST_CASE(tcs, filters_state__unused__none);
    ATF_ADD_TEST_CASE(tcs, filters_state__unused__some);
}

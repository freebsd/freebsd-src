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

#include "utils/format/containers.ipp"

#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <atf-c++.hpp>



namespace {


/// Formats a value and compares it to an expected string.
///
/// \tparam T The type of the value to format.
/// \param expected Expected formatted text.
/// \param actual The value to format.
///
/// \post Fails the test case if the formatted actual value does not match
/// the provided expected string.
template< typename T >
static void
do_check(const char* expected, const T& actual)
{
    std::ostringstream str;
    str << actual;
    ATF_REQUIRE_EQ(expected, str.str());
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(std_map__empty);
ATF_TEST_CASE_BODY(std_map__empty)
{
    do_check("map()", std::map< char, char >());
    do_check("map()", std::map< int, long >());
}


ATF_TEST_CASE_WITHOUT_HEAD(std_map__some);
ATF_TEST_CASE_BODY(std_map__some)
{
    {
        std::map< char, int > v;
        v['b'] = 123;
        v['z'] = 321;
        do_check("map(b=123, z=321)", v);
    }

    {
        std::map< int, std::string > v;
        v[5] = "first";
        v[2] = "second";
        v[8] = "third";
        do_check("map(2=second, 5=first, 8=third)", v);
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(std_pair);
ATF_TEST_CASE_BODY(std_pair)
{
    do_check("pair(5, b)", std::pair< int, char >(5, 'b'));
    do_check("pair(foo bar, baz)",
             std::pair< std::string, std::string >("foo bar", "baz"));
}


ATF_TEST_CASE_WITHOUT_HEAD(std_shared_ptr__null);
ATF_TEST_CASE_BODY(std_shared_ptr__null)
{
    do_check("<NULL>", std::shared_ptr< char >());
    do_check("<NULL>", std::shared_ptr< int >());
}


ATF_TEST_CASE_WITHOUT_HEAD(std_shared_ptr__not_null);
ATF_TEST_CASE_BODY(std_shared_ptr__not_null)
{
    do_check("f", std::shared_ptr< char >(new char('f')));
    do_check("8", std::shared_ptr< int >(new int(8)));
}


ATF_TEST_CASE_WITHOUT_HEAD(std_set__empty);
ATF_TEST_CASE_BODY(std_set__empty)
{
    do_check("set()", std::set< char >());
    do_check("set()", std::set< int >());
}


ATF_TEST_CASE_WITHOUT_HEAD(std_set__some);
ATF_TEST_CASE_BODY(std_set__some)
{
    {
        std::set< char > v;
        v.insert('b');
        v.insert('z');
        do_check("set(b, z)", v);
    }

    {
        std::set< int > v;
        v.insert(5);
        v.insert(2);
        v.insert(8);
        do_check("set(2, 5, 8)", v);
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(std_vector__empty);
ATF_TEST_CASE_BODY(std_vector__empty)
{
    do_check("[]", std::vector< char >());
    do_check("[]", std::vector< int >());
}


ATF_TEST_CASE_WITHOUT_HEAD(std_vector__some);
ATF_TEST_CASE_BODY(std_vector__some)
{
    {
        std::vector< char > v;
        v.push_back('b');
        v.push_back('z');
        do_check("[b, z]", v);
    }

    {
        std::vector< int > v;
        v.push_back(5);
        v.push_back(2);
        v.push_back(8);
        do_check("[5, 2, 8]", v);
    }
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, std_map__empty);
    ATF_ADD_TEST_CASE(tcs, std_map__some);

    ATF_ADD_TEST_CASE(tcs, std_pair);

    ATF_ADD_TEST_CASE(tcs, std_shared_ptr__null);
    ATF_ADD_TEST_CASE(tcs, std_shared_ptr__not_null);

    ATF_ADD_TEST_CASE(tcs, std_set__empty);
    ATF_ADD_TEST_CASE(tcs, std_set__some);

    ATF_ADD_TEST_CASE(tcs, std_vector__empty);
    ATF_ADD_TEST_CASE(tcs, std_vector__some);
}

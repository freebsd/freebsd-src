// Copyright 2024 The Kyua Authors.
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

#include "engine/googletest_list.hpp"

#include <sstream>
#include <string>

#include <atf-c++.hpp>

#include "engine/exceptions.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/types.hpp"
#include "utils/datetime.hpp"
#include "utils/format/containers.ipp"
#include "utils/fs/path.hpp"
#include "utils/units.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace units = utils::units;


ATF_TEST_CASE_WITHOUT_HEAD(parse_googletest_list__invalid_testcase_definition);
ATF_TEST_CASE_BODY(parse_googletest_list__invalid_testcase_definition)
{
    std::istringstream input1("  \n");

    ATF_REQUIRE_THROW_RE(engine::format_error, "No test cases",
        engine::parse_googletest_list(input1));

    std::istringstream input2("  TestcaseWithoutSuite\n");
    ATF_REQUIRE_THROW_RE(engine::format_error,
        "Invalid testcase definition: not preceded by a test suite definition",
        engine::parse_googletest_list(input2));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_googletest_list__invalid_testsuite_definition);
ATF_TEST_CASE_BODY(parse_googletest_list__invalid_testsuite_definition)
{
    std::istringstream input1("\n");
    ATF_REQUIRE_THROW_RE(engine::format_error, "No test cases",
        engine::parse_googletest_list(input1));

    std::istringstream input2(
"TestSuiteWithoutSeparator\n"
    );
    ATF_REQUIRE_THROW_RE(engine::format_error, "No test cases",
        engine::parse_googletest_list(input2));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_googletest_list__no_test_cases);
ATF_TEST_CASE_BODY(parse_googletest_list__no_test_cases)
{
    std::istringstream input("");
    ATF_REQUIRE_THROW_RE(engine::format_error, "No test cases",
        engine::parse_googletest_list(input));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_googletest_list__one_test_case);
ATF_TEST_CASE_BODY(parse_googletest_list__one_test_case)
{
    std::istringstream input(
"TestSuite.\n"
"  TestCase\n"
    );
    const model::test_cases_map tests = engine::parse_googletest_list(input);

    const model::test_cases_map exp_tests = model::test_cases_map_builder()
        .add("TestSuite.TestCase").build();
    ATF_REQUIRE_EQ(exp_tests, tests);
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_googletest_list__one_parameterized_test_case);
ATF_TEST_CASE_BODY(parse_googletest_list__one_parameterized_test_case)
{
    std::istringstream input(
"TestSuite.\n"
"  TestCase/0  # GetParam() = 'c'\n"
    );
    const model::test_cases_map tests = engine::parse_googletest_list(input);

    const model::test_cases_map exp_tests = model::test_cases_map_builder()
        .add("TestSuite.TestCase/0").build();
    ATF_REQUIRE_EQ(exp_tests, tests);
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_googletest_list__one_parameterized_test_suite);
ATF_TEST_CASE_BODY(parse_googletest_list__one_parameterized_test_suite)
{
    std::string text =
"TestSuite/0.  # TypeParam = int\n"
"  TestCase\n"
    ;
    std::istringstream input(text);
    const model::test_cases_map tests = engine::parse_googletest_list(input);

    const model::test_cases_map exp_tests = model::test_cases_map_builder()
        .add("TestSuite/0.TestCase").build();
    ATF_REQUIRE_EQ(exp_tests, tests);
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_googletest_list__one_parameterized_test_case_and_test_suite);
ATF_TEST_CASE_BODY(parse_googletest_list__one_parameterized_test_case_and_test_suite)
{
    std::string text =
"TestSuite/0.  # TypeParam = int\n"
"  TestCase/0  # GetParam() = \"herp\"\n"
"  TestCase/1  # GetParam() = \"derp\"\n"
    ;
    std::istringstream input(text);
    const model::test_cases_map tests = engine::parse_googletest_list(input);

    const model::test_cases_map exp_tests = model::test_cases_map_builder()
        .add("TestSuite/0.TestCase/0")
        .add("TestSuite/0.TestCase/1")
        .build();
    ATF_REQUIRE_EQ(exp_tests, tests);
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_googletest_list__many_test_cases);
ATF_TEST_CASE_BODY(parse_googletest_list__many_test_cases)
{
    std::string text =
"FirstTestSuite.\n"
"  ATestCase\n"
"SecondTestSuite.\n"
"  AnotherTestCase\n"
"ThirdTestSuite.\n"
"  _\n"
"FourthTestSuite/0.  # TypeParam = std::list<int>\n"
"  TestCase\n"
"FourthTestSuite/1.  # TypeParam = std::list<int>\n"
"  TestCase\n"
"FifthTestSuite.\n"
"  TestCase/0  # GetParam() = 0\n"
"  TestCase/1  # GetParam() = (1, 2, 3)\n"
"  TestCase/2  # GetParam() = \"developers. developers\"\n"
"SixthTestSuite/0.  # TypeParam = std::map<std::basic_string, int>\n"
"  TestCase/0  # GetParam() = 0\n"
"  TestCase/1  # GetParam() = (1, 2, 3)\n"
    ;
    std::istringstream input(text);
    const model::test_cases_map tests = engine::parse_googletest_list(input);

    const model::test_cases_map exp_tests = model::test_cases_map_builder()
        .add("FirstTestSuite.ATestCase", model::metadata_builder()
             .build())
        .add("SecondTestSuite.AnotherTestCase", model::metadata_builder()
             .build())
        .add("ThirdTestSuite._")
        .add("FourthTestSuite/0.TestCase")
        .add("FourthTestSuite/1.TestCase")
        .add("FifthTestSuite.TestCase/0")
        .add("FifthTestSuite.TestCase/1")
        .add("FifthTestSuite.TestCase/2")
        .add("SixthTestSuite/0.TestCase/0")
        .add("SixthTestSuite/0.TestCase/1")
        .build();
    ATF_REQUIRE_EQ(exp_tests, tests);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, parse_googletest_list__invalid_testcase_definition);
    ATF_ADD_TEST_CASE(tcs, parse_googletest_list__invalid_testsuite_definition);
    ATF_ADD_TEST_CASE(tcs, parse_googletest_list__no_test_cases);
    ATF_ADD_TEST_CASE(tcs, parse_googletest_list__one_test_case);
    ATF_ADD_TEST_CASE(tcs, parse_googletest_list__one_parameterized_test_case);
    ATF_ADD_TEST_CASE(tcs, parse_googletest_list__one_parameterized_test_suite);
    ATF_ADD_TEST_CASE(tcs,
        parse_googletest_list__one_parameterized_test_case_and_test_suite);
    ATF_ADD_TEST_CASE(tcs, parse_googletest_list__many_test_cases);
}

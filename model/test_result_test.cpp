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

#include "model/test_result.hpp"

#include <sstream>

#include <atf-c++.hpp>


/// Creates a test case to validate the getters.
///
/// \param name The name of the test case; "__getters" will be appended.
/// \param expected_type The expected type of the result.
/// \param expected_reason The expected reason for the result.
/// \param result The result to query.
#define GETTERS_TEST(name, expected_type, expected_reason, result) \
    ATF_TEST_CASE_WITHOUT_HEAD(name ## __getters); \
    ATF_TEST_CASE_BODY(name ## __getters) \
    { \
        ATF_REQUIRE(expected_type == result.type()); \
        ATF_REQUIRE_EQ(expected_reason, result.reason());  \
    }


/// Creates a test case to validate the good() method.
///
/// \param name The name of the test case; "__good" will be appended.
/// \param expected The expected result of good().
/// \param result_type The result type to check.
#define GOOD_TEST(name, expected, result_type) \
    ATF_TEST_CASE_WITHOUT_HEAD(name ## __good); \
    ATF_TEST_CASE_BODY(name ## __good) \
    { \
        ATF_REQUIRE_EQ(expected, model::test_result(result_type).good()); \
    }


/// Creates a test case to validate the operator<< method.
///
/// \param name The name of the test case; "__output" will be appended.
/// \param expected The expected string in the output.
/// \param result The result to format.
#define OUTPUT_TEST(name, expected, result) \
    ATF_TEST_CASE_WITHOUT_HEAD(name ## __output); \
    ATF_TEST_CASE_BODY(name ## __output) \
    { \
        std::ostringstream output; \
        output << "prefix" << result << "suffix"; \
        ATF_REQUIRE_EQ("prefix" + std::string(expected) + "suffix", \
                       output.str()); \
    }


GETTERS_TEST(
    broken,
    model::test_result_broken,
    "The reason",
    model::test_result(model::test_result_broken, "The reason"));
GETTERS_TEST(
    expected_failure,
    model::test_result_expected_failure,
    "The reason",
    model::test_result(model::test_result_expected_failure, "The reason"));
GETTERS_TEST(
    failed,
    model::test_result_failed,
    "The reason",
    model::test_result(model::test_result_failed, "The reason"));
GETTERS_TEST(
    passed,
    model::test_result_passed,
    "",
    model::test_result(model::test_result_passed));
GETTERS_TEST(
    skipped,
    model::test_result_skipped,
    "The reason",
    model::test_result(model::test_result_skipped, "The reason"));


GOOD_TEST(broken, false, model::test_result_broken);
GOOD_TEST(expected_failure, true, model::test_result_expected_failure);
GOOD_TEST(failed, false, model::test_result_failed);
GOOD_TEST(passed, true, model::test_result_passed);
GOOD_TEST(skipped, true, model::test_result_skipped);


OUTPUT_TEST(
    broken,
    "model::test_result{type='broken', reason='foo'}",
    model::test_result(model::test_result_broken, "foo"));
OUTPUT_TEST(
    expected_failure,
    "model::test_result{type='expected_failure', reason='abc def'}",
    model::test_result(model::test_result_expected_failure, "abc def"));
OUTPUT_TEST(
    failed,
    "model::test_result{type='failed', reason='some \\'string'}",
    model::test_result(model::test_result_failed, "some 'string"));
OUTPUT_TEST(
    passed,
    "model::test_result{type='passed'}",
    model::test_result(model::test_result_passed, ""));
OUTPUT_TEST(
    skipped,
    "model::test_result{type='skipped', reason='last message'}",
    model::test_result(model::test_result_skipped, "last message"));


ATF_TEST_CASE_WITHOUT_HEAD(operator_eq);
ATF_TEST_CASE_BODY(operator_eq)
{
    const model::test_result result1(model::test_result_broken, "Foo");
    const model::test_result result2(model::test_result_broken, "Foo");
    const model::test_result result3(model::test_result_broken, "Bar");
    const model::test_result result4(model::test_result_failed, "Foo");

    ATF_REQUIRE(  result1 == result1);
    ATF_REQUIRE(  result1 == result2);
    ATF_REQUIRE(!(result1 == result3));
    ATF_REQUIRE(!(result1 == result4));
}


ATF_TEST_CASE_WITHOUT_HEAD(operator_ne);
ATF_TEST_CASE_BODY(operator_ne)
{
    const model::test_result result1(model::test_result_broken, "Foo");
    const model::test_result result2(model::test_result_broken, "Foo");
    const model::test_result result3(model::test_result_broken, "Bar");
    const model::test_result result4(model::test_result_failed, "Foo");

    ATF_REQUIRE(!(result1 != result1));
    ATF_REQUIRE(!(result1 != result2));
    ATF_REQUIRE(  result1 != result3);
    ATF_REQUIRE(  result1 != result4);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, broken__getters);
    ATF_ADD_TEST_CASE(tcs, broken__good);
    ATF_ADD_TEST_CASE(tcs, broken__output);
    ATF_ADD_TEST_CASE(tcs, expected_failure__getters);
    ATF_ADD_TEST_CASE(tcs, expected_failure__good);
    ATF_ADD_TEST_CASE(tcs, expected_failure__output);
    ATF_ADD_TEST_CASE(tcs, failed__getters);
    ATF_ADD_TEST_CASE(tcs, failed__good);
    ATF_ADD_TEST_CASE(tcs, failed__output);
    ATF_ADD_TEST_CASE(tcs, passed__getters);
    ATF_ADD_TEST_CASE(tcs, passed__good);
    ATF_ADD_TEST_CASE(tcs, passed__output);
    ATF_ADD_TEST_CASE(tcs, skipped__getters);
    ATF_ADD_TEST_CASE(tcs, skipped__good);
    ATF_ADD_TEST_CASE(tcs, skipped__output);
    ATF_ADD_TEST_CASE(tcs, operator_eq);
    ATF_ADD_TEST_CASE(tcs, operator_ne);
}

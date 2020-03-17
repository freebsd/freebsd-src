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

#include "engine/atf_result.hpp"

extern "C" {
#include <signal.h>
}

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <atf-c++.hpp>

#include "engine/exceptions.hpp"
#include "model/test_result.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/process/status.hpp"

namespace fs = utils::fs;
namespace process = utils::process;

using utils::none;
using utils::optional;


namespace {


/// Performs a test for results::parse() that should succeed.
///
/// \param exp_type The expected type of the result.
/// \param exp_argument The expected argument in the result, if any.
/// \param exp_reason The expected reason describing the result, if any.
/// \param text The literal input to parse; can include multiple lines.
static void
parse_ok_test(const engine::atf_result::types& exp_type,
              const optional< int >& exp_argument,
              const char* exp_reason, const char* text)
{
    std::istringstream input(text);
    const engine::atf_result actual = engine::atf_result::parse(input);
    ATF_REQUIRE(exp_type == actual.type());
    ATF_REQUIRE_EQ(exp_argument, actual.argument());
    if (exp_reason != NULL) {
        ATF_REQUIRE(actual.reason());
        ATF_REQUIRE_EQ(exp_reason, actual.reason().get());
    } else {
        ATF_REQUIRE(!actual.reason());
    }
}


/// Wrapper around parse_ok_test to define a test case.
///
/// \param name The name of the test case; will be prefixed with
///     "atf_result__parse__".
/// \param exp_type The expected type of the result.
/// \param exp_argument The expected argument in the result, if any.
/// \param exp_reason The expected reason describing the result, if any.
/// \param input The literal input to parse.
#define PARSE_OK(name, exp_type, exp_argument, exp_reason, input) \
    ATF_TEST_CASE_WITHOUT_HEAD(atf_result__parse__ ## name); \
    ATF_TEST_CASE_BODY(atf_result__parse__ ## name) \
    { \
        parse_ok_test(exp_type, exp_argument, exp_reason, input); \
    }


/// Performs a test for results::parse() that should fail.
///
/// \param reason_regexp The reason to match against the broken reason.
/// \param text The literal input to parse; can include multiple lines.
static void
parse_broken_test(const char* reason_regexp, const char* text)
{
    std::istringstream input(text);
    ATF_REQUIRE_THROW_RE(engine::format_error, reason_regexp,
                         engine::atf_result::parse(input));
}


/// Wrapper around parse_broken_test to define a test case.
///
/// \param name The name of the test case; will be prefixed with
///    "atf_result__parse__".
/// \param reason_regexp The reason to match against the broken reason.
/// \param input The literal input to parse.
#define PARSE_BROKEN(name, reason_regexp, input) \
    ATF_TEST_CASE_WITHOUT_HEAD(atf_result__parse__ ## name); \
    ATF_TEST_CASE_BODY(atf_result__parse__ ## name) \
    { \
        parse_broken_test(reason_regexp, input); \
    }


}  // anonymous namespace


PARSE_BROKEN(empty,
             "Empty.*no new line",
             "");
PARSE_BROKEN(no_newline__unknown,
             "Empty.*no new line",
             "foo");
PARSE_BROKEN(no_newline__known,
             "Empty.*no new line",
             "passed");
PARSE_BROKEN(multiline__no_newline,
             "multiple lines.*foo<<NEWLINE>>bar",
             "failed: foo\nbar");
PARSE_BROKEN(multiline__with_newline,
             "multiple lines.*foo<<NEWLINE>>bar",
             "failed: foo\nbar\n");
PARSE_BROKEN(unknown_status__no_reason,
             "Unknown.*result.*'cba'",
             "cba\n");
PARSE_BROKEN(unknown_status__with_reason,
             "Unknown.*result.*'hgf'",
             "hgf: foo\n");
PARSE_BROKEN(missing_reason__no_delim,
             "failed.*followed by.*reason",
             "failed\n");
PARSE_BROKEN(missing_reason__bad_delim,
             "failed.*followed by.*reason",
             "failed:\n");
PARSE_BROKEN(missing_reason__empty,
             "failed.*followed by.*reason",
             "failed: \n");


PARSE_OK(broken__ok,
         engine::atf_result::broken, none, "a b c",
         "broken: a b c\n");
PARSE_OK(broken__blanks,
         engine::atf_result::broken, none, "   ",
         "broken:    \n");


PARSE_OK(expected_death__ok,
         engine::atf_result::expected_death, none, "a b c",
         "expected_death: a b c\n");
PARSE_OK(expected_death__blanks,
         engine::atf_result::expected_death, none, "   ",
         "expected_death:    \n");


PARSE_OK(expected_exit__ok__any,
         engine::atf_result::expected_exit, none, "any exit code",
         "expected_exit: any exit code\n");
PARSE_OK(expected_exit__ok__specific,
         engine::atf_result::expected_exit, optional< int >(712),
         "some known exit code",
         "expected_exit(712): some known exit code\n");
PARSE_BROKEN(expected_exit__bad_int,
             "Invalid integer.*45a3",
             "expected_exit(45a3): this is broken\n");


PARSE_OK(expected_failure__ok,
         engine::atf_result::expected_failure, none, "a b c",
         "expected_failure: a b c\n");
PARSE_OK(expected_failure__blanks,
         engine::atf_result::expected_failure, none, "   ",
         "expected_failure:    \n");


PARSE_OK(expected_signal__ok__any,
         engine::atf_result::expected_signal, none, "any signal code",
         "expected_signal: any signal code\n");
PARSE_OK(expected_signal__ok__specific,
         engine::atf_result::expected_signal, optional< int >(712),
         "some known signal code",
         "expected_signal(712): some known signal code\n");
PARSE_BROKEN(expected_signal__bad_int,
             "Invalid integer.*45a3",
             "expected_signal(45a3): this is broken\n");


PARSE_OK(expected_timeout__ok,
         engine::atf_result::expected_timeout, none, "a b c",
         "expected_timeout: a b c\n");
PARSE_OK(expected_timeout__blanks,
         engine::atf_result::expected_timeout, none, "   ",
         "expected_timeout:    \n");


PARSE_OK(failed__ok,
         engine::atf_result::failed, none, "a b c",
         "failed: a b c\n");
PARSE_OK(failed__blanks,
         engine::atf_result::failed, none, "   ",
         "failed:    \n");


PARSE_OK(passed__ok,
         engine::atf_result::passed, none, NULL,
         "passed\n");
PARSE_BROKEN(passed__reason,
             "cannot have a reason",
             "passed a b c\n");


PARSE_OK(skipped__ok,
         engine::atf_result::skipped, none, "a b c",
         "skipped: a b c\n");
PARSE_OK(skipped__blanks,
         engine::atf_result::skipped, none, "   ",
         "skipped:    \n");


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__load__ok);
ATF_TEST_CASE_BODY(atf_result__load__ok)
{
    std::ofstream output("result.txt");
    ATF_REQUIRE(output);
    output << "skipped: a b c\n";
    output.close();

    const engine::atf_result result = engine::atf_result::load(
        utils::fs::path("result.txt"));
    ATF_REQUIRE(engine::atf_result::skipped == result.type());
    ATF_REQUIRE(!result.argument());
    ATF_REQUIRE(result.reason());
    ATF_REQUIRE_EQ("a b c", result.reason().get());
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__load__missing_file);
ATF_TEST_CASE_BODY(atf_result__load__missing_file)
{
    ATF_REQUIRE_THROW_RE(
        std::runtime_error, "Cannot open",
        engine::atf_result::load(utils::fs::path("result.txt")));
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__load__format_error);
ATF_TEST_CASE_BODY(atf_result__load__format_error)
{
    std::ofstream output("abc.txt");
    ATF_REQUIRE(output);
    output << "passed: foo\n";
    output.close();

    ATF_REQUIRE_THROW_RE(engine::format_error, "cannot have a reason",
                         engine::atf_result::load(utils::fs::path("abc.txt")));
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__apply__broken__ok);
ATF_TEST_CASE_BODY(atf_result__apply__broken__ok)
{
    const engine::atf_result in_result(engine::atf_result::broken,
                                       "Passthrough");
    const process::status status = process::status::fake_exited(EXIT_SUCCESS);
    ATF_REQUIRE_EQ(in_result, in_result.apply(utils::make_optional(status)));
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__apply__timed_out);
ATF_TEST_CASE_BODY(atf_result__apply__timed_out)
{
    const engine::atf_result timed_out(engine::atf_result::broken,
                                       "Some arbitrary error");
    ATF_REQUIRE_EQ(engine::atf_result(engine::atf_result::broken,
                                      "Test case body timed out"),
                   timed_out.apply(none));
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__apply__expected_death__ok);
ATF_TEST_CASE_BODY(atf_result__apply__expected_death__ok)
{
    const engine::atf_result in_result(engine::atf_result::expected_death,
                                       "Passthrough");
    const process::status status = process::status::fake_signaled(SIGINT, true);
    ATF_REQUIRE_EQ(in_result, in_result.apply(utils::make_optional(status)));
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__apply__expected_exit__ok);
ATF_TEST_CASE_BODY(atf_result__apply__expected_exit__ok)
{
    const process::status success = process::status::fake_exited(EXIT_SUCCESS);
    const process::status failure = process::status::fake_exited(EXIT_FAILURE);

    const engine::atf_result any_code(engine::atf_result::expected_exit, none,
                                      "The reason");
    ATF_REQUIRE_EQ(any_code, any_code.apply(utils::make_optional(success)));
    ATF_REQUIRE_EQ(any_code, any_code.apply(utils::make_optional(failure)));

    const engine::atf_result a_code(engine::atf_result::expected_exit,
                            utils::make_optional(EXIT_FAILURE), "The reason");
    ATF_REQUIRE_EQ(a_code, a_code.apply(utils::make_optional(failure)));
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__apply__expected_exit__failed);
ATF_TEST_CASE_BODY(atf_result__apply__expected_exit__failed)
{
    const process::status success = process::status::fake_exited(EXIT_SUCCESS);

    const engine::atf_result a_code(engine::atf_result::expected_exit,
                            utils::make_optional(EXIT_FAILURE), "The reason");
    ATF_REQUIRE_EQ(
        engine::atf_result(engine::atf_result::failed,
                           "Test case expected to exit with code 1 but got "
                           "code 0"),
        a_code.apply(utils::make_optional(success)));
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__apply__expected_exit__broken);
ATF_TEST_CASE_BODY(atf_result__apply__expected_exit__broken)
{
    const process::status sig3 = process::status::fake_signaled(3, false);

    const engine::atf_result any_code(engine::atf_result::expected_exit, none,
                                      "The reason");
    ATF_REQUIRE_EQ(
        engine::atf_result(engine::atf_result::broken,
                           "Expected clean exit but received signal 3"),
        any_code.apply(utils::make_optional(sig3)));
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__apply__expected_failure__ok);
ATF_TEST_CASE_BODY(atf_result__apply__expected_failure__ok)
{
    const process::status status = process::status::fake_exited(EXIT_SUCCESS);
    const engine::atf_result xfailure(engine::atf_result::expected_failure,
                                      "The reason");
    ATF_REQUIRE_EQ(xfailure, xfailure.apply(utils::make_optional(status)));
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__apply__expected_failure__broken);
ATF_TEST_CASE_BODY(atf_result__apply__expected_failure__broken)
{
    const process::status failure = process::status::fake_exited(EXIT_FAILURE);
    const process::status sig3 = process::status::fake_signaled(3, true);
    const process::status sig4 = process::status::fake_signaled(4, false);

    const engine::atf_result xfailure(engine::atf_result::expected_failure,
                                      "The reason");
    ATF_REQUIRE_EQ(
        engine::atf_result(engine::atf_result::broken,
                           "Expected failure should have reported success but "
                           "exited with code 1"),
        xfailure.apply(utils::make_optional(failure)));
    ATF_REQUIRE_EQ(
        engine::atf_result(engine::atf_result::broken,
                           "Expected failure should have reported success but "
                           "received signal 3 (core dumped)"),
        xfailure.apply(utils::make_optional(sig3)));
    ATF_REQUIRE_EQ(
        engine::atf_result(engine::atf_result::broken,
                           "Expected failure should have reported success but "
                           "received signal 4"),
        xfailure.apply(utils::make_optional(sig4)));
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__apply__expected_signal__ok);
ATF_TEST_CASE_BODY(atf_result__apply__expected_signal__ok)
{
    const process::status sig1 = process::status::fake_signaled(1, false);
    const process::status sig3 = process::status::fake_signaled(3, true);

    const engine::atf_result any_sig(engine::atf_result::expected_signal, none,
                                     "The reason");
    ATF_REQUIRE_EQ(any_sig, any_sig.apply(utils::make_optional(sig1)));
    ATF_REQUIRE_EQ(any_sig, any_sig.apply(utils::make_optional(sig3)));

    const engine::atf_result a_sig(engine::atf_result::expected_signal,
                           utils::make_optional(3), "The reason");
    ATF_REQUIRE_EQ(a_sig, a_sig.apply(utils::make_optional(sig3)));
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__apply__expected_signal__failed);
ATF_TEST_CASE_BODY(atf_result__apply__expected_signal__failed)
{
    const process::status sig5 = process::status::fake_signaled(5, false);

    const engine::atf_result a_sig(engine::atf_result::expected_signal,
                           utils::make_optional(4), "The reason");
    ATF_REQUIRE_EQ(
        engine::atf_result(engine::atf_result::failed,
                           "Test case expected to receive signal 4 but got 5"),
        a_sig.apply(utils::make_optional(sig5)));
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__apply__expected_signal__broken);
ATF_TEST_CASE_BODY(atf_result__apply__expected_signal__broken)
{
    const process::status success = process::status::fake_exited(EXIT_SUCCESS);

    const engine::atf_result any_sig(engine::atf_result::expected_signal, none,
                                     "The reason");
    ATF_REQUIRE_EQ(
        engine::atf_result(engine::atf_result::broken,
                           "Expected signal but exited with code 0"),
        any_sig.apply(utils::make_optional(success)));
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__apply__expected_timeout__ok);
ATF_TEST_CASE_BODY(atf_result__apply__expected_timeout__ok)
{
    const engine::atf_result timeout(engine::atf_result::expected_timeout,
                                     "The reason");
    ATF_REQUIRE_EQ(timeout, timeout.apply(none));
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__apply__expected_timeout__broken);
ATF_TEST_CASE_BODY(atf_result__apply__expected_timeout__broken)
{
    const process::status status = process::status::fake_exited(EXIT_SUCCESS);
    const engine::atf_result timeout(engine::atf_result::expected_timeout,
                                     "The reason");
    ATF_REQUIRE_EQ(
        engine::atf_result(engine::atf_result::broken,
                           "Expected timeout but exited with code 0"),
        timeout.apply(utils::make_optional(status)));
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__apply__failed__ok);
ATF_TEST_CASE_BODY(atf_result__apply__failed__ok)
{
    const process::status status = process::status::fake_exited(EXIT_FAILURE);
    const engine::atf_result failed(engine::atf_result::failed, "The reason");
    ATF_REQUIRE_EQ(failed, failed.apply(utils::make_optional(status)));
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__apply__failed__broken);
ATF_TEST_CASE_BODY(atf_result__apply__failed__broken)
{
    const process::status success = process::status::fake_exited(EXIT_SUCCESS);
    const process::status sig3 = process::status::fake_signaled(3, true);
    const process::status sig4 = process::status::fake_signaled(4, false);

    const engine::atf_result failed(engine::atf_result::failed, "The reason");
    ATF_REQUIRE_EQ(
        engine::atf_result(engine::atf_result::broken,
                           "Failed test case should have reported failure but "
                           "exited with code 0"),
        failed.apply(utils::make_optional(success)));
    ATF_REQUIRE_EQ(
        engine::atf_result(engine::atf_result::broken,
                           "Failed test case should have reported failure but "
                           "received signal 3 (core dumped)"),
        failed.apply(utils::make_optional(sig3)));
    ATF_REQUIRE_EQ(
        engine::atf_result(engine::atf_result::broken,
                           "Failed test case should have reported failure but "
                           "received signal 4"),
        failed.apply(utils::make_optional(sig4)));
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__apply__passed__ok);
ATF_TEST_CASE_BODY(atf_result__apply__passed__ok)
{
    const process::status status = process::status::fake_exited(EXIT_SUCCESS);
    const engine::atf_result passed(engine::atf_result::passed);
    ATF_REQUIRE_EQ(passed, passed.apply(utils::make_optional(status)));
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__apply__passed__broken);
ATF_TEST_CASE_BODY(atf_result__apply__passed__broken)
{
    const process::status failure = process::status::fake_exited(EXIT_FAILURE);
    const process::status sig3 = process::status::fake_signaled(3, true);
    const process::status sig4 = process::status::fake_signaled(4, false);

    const engine::atf_result passed(engine::atf_result::passed);
    ATF_REQUIRE_EQ(
        engine::atf_result(engine::atf_result::broken,
                           "Passed test case should have reported success but "
                           "exited with code 1"),
        passed.apply(utils::make_optional(failure)));
    ATF_REQUIRE_EQ(
        engine::atf_result(engine::atf_result::broken,
                           "Passed test case should have reported success but "
                           "received signal 3 (core dumped)"),
        passed.apply(utils::make_optional(sig3)));
    ATF_REQUIRE_EQ(
        engine::atf_result(engine::atf_result::broken,
                           "Passed test case should have reported success but "
                           "received signal 4"),
        passed.apply(utils::make_optional(sig4)));
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__apply__skipped__ok);
ATF_TEST_CASE_BODY(atf_result__apply__skipped__ok)
{
    const process::status status = process::status::fake_exited(EXIT_SUCCESS);
    const engine::atf_result skipped(engine::atf_result::skipped, "The reason");
    ATF_REQUIRE_EQ(skipped, skipped.apply(utils::make_optional(status)));
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__apply__skipped__broken);
ATF_TEST_CASE_BODY(atf_result__apply__skipped__broken)
{
    const process::status failure = process::status::fake_exited(EXIT_FAILURE);
    const process::status sig3 = process::status::fake_signaled(3, true);
    const process::status sig4 = process::status::fake_signaled(4, false);

    const engine::atf_result skipped(engine::atf_result::skipped, "The reason");
    ATF_REQUIRE_EQ(
        engine::atf_result(engine::atf_result::broken,
                           "Skipped test case should have reported success but "
                           "exited with code 1"),
        skipped.apply(utils::make_optional(failure)));
    ATF_REQUIRE_EQ(
        engine::atf_result(engine::atf_result::broken,
                           "Skipped test case should have reported success but "
                           "received signal 3 (core dumped)"),
        skipped.apply(utils::make_optional(sig3)));
    ATF_REQUIRE_EQ(
        engine::atf_result(engine::atf_result::broken,
                           "Skipped test case should have reported success but "
                           "received signal 4"),
        skipped.apply(utils::make_optional(sig4)));
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__externalize__broken);
ATF_TEST_CASE_BODY(atf_result__externalize__broken)
{
    const engine::atf_result raw(engine::atf_result::broken, "The reason");
    const model::test_result expected(model::test_result_broken,
                                      "The reason");
    ATF_REQUIRE_EQ(expected, raw.externalize());
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__externalize__expected_death);
ATF_TEST_CASE_BODY(atf_result__externalize__expected_death)
{
    const engine::atf_result raw(engine::atf_result::expected_death,
                                 "The reason");
    const model::test_result expected(model::test_result_expected_failure,
                                      "The reason");
    ATF_REQUIRE_EQ(expected, raw.externalize());
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__externalize__expected_exit);
ATF_TEST_CASE_BODY(atf_result__externalize__expected_exit)
{
    const engine::atf_result raw(engine::atf_result::expected_exit,
                                 "The reason");
    const model::test_result expected(model::test_result_expected_failure,
                                      "The reason");
    ATF_REQUIRE_EQ(expected, raw.externalize());
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__externalize__expected_failure);
ATF_TEST_CASE_BODY(atf_result__externalize__expected_failure)
{
    const engine::atf_result raw(engine::atf_result::expected_failure,
                                 "The reason");
    const model::test_result expected(model::test_result_expected_failure,
                                      "The reason");
    ATF_REQUIRE_EQ(expected, raw.externalize());
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__externalize__expected_signal);
ATF_TEST_CASE_BODY(atf_result__externalize__expected_signal)
{
    const engine::atf_result raw(engine::atf_result::expected_signal,
                                 "The reason");
    const model::test_result expected(model::test_result_expected_failure,
                                      "The reason");
    ATF_REQUIRE_EQ(expected, raw.externalize());
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__externalize__expected_timeout);
ATF_TEST_CASE_BODY(atf_result__externalize__expected_timeout)
{
    const engine::atf_result raw(engine::atf_result::expected_timeout,
                                 "The reason");
    const model::test_result expected(model::test_result_expected_failure,
                                      "The reason");
    ATF_REQUIRE_EQ(expected, raw.externalize());
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__externalize__failed);
ATF_TEST_CASE_BODY(atf_result__externalize__failed)
{
    const engine::atf_result raw(engine::atf_result::failed, "The reason");
    const model::test_result expected(model::test_result_failed,
                                      "The reason");
    ATF_REQUIRE(expected == raw.externalize());
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__externalize__passed);
ATF_TEST_CASE_BODY(atf_result__externalize__passed)
{
    const engine::atf_result raw(engine::atf_result::passed);
    const model::test_result expected(model::test_result_passed);
    ATF_REQUIRE_EQ(expected, raw.externalize());
}


ATF_TEST_CASE_WITHOUT_HEAD(atf_result__externalize__skipped);
ATF_TEST_CASE_BODY(atf_result__externalize__skipped)
{
    const engine::atf_result raw(engine::atf_result::skipped, "The reason");
    const model::test_result expected(model::test_result_skipped,
                                      "The reason");
    ATF_REQUIRE_EQ(expected, raw.externalize());
}


ATF_TEST_CASE_WITHOUT_HEAD(calculate_atf_result__missing_file);
ATF_TEST_CASE_BODY(calculate_atf_result__missing_file)
{
    using process::status;

    const status body_status = status::fake_exited(EXIT_SUCCESS);
    const model::test_result expected(
        model::test_result_broken,
        "Premature exit; test case exited with code 0");
    ATF_REQUIRE_EQ(expected, engine::calculate_atf_result(
        utils::make_optional(body_status), fs::path("foo")));
}


ATF_TEST_CASE_WITHOUT_HEAD(calculate_atf_result__bad_file);
ATF_TEST_CASE_BODY(calculate_atf_result__bad_file)
{
    using process::status;

    const status body_status = status::fake_exited(EXIT_SUCCESS);
    atf::utils::create_file("foo", "invalid\n");
    const model::test_result expected(model::test_result_broken,
                                      "Unknown test result 'invalid'");
    ATF_REQUIRE_EQ(expected, engine::calculate_atf_result(
        utils::make_optional(body_status), fs::path("foo")));
}


ATF_TEST_CASE_WITHOUT_HEAD(calculate_atf_result__body_ok);
ATF_TEST_CASE_BODY(calculate_atf_result__body_ok)
{
    using process::status;

    atf::utils::create_file("result.txt", "skipped: Something\n");
    const status body_status = status::fake_exited(EXIT_SUCCESS);
    ATF_REQUIRE_EQ(
        model::test_result(model::test_result_skipped, "Something"),
        engine::calculate_atf_result(utils::make_optional(body_status),
                                     fs::path("result.txt")));
}


ATF_TEST_CASE_WITHOUT_HEAD(calculate_atf_result__body_bad);
ATF_TEST_CASE_BODY(calculate_atf_result__body_bad)
{
    using process::status;

    atf::utils::create_file("result.txt", "skipped: Something\n");
    const status body_status = status::fake_exited(EXIT_FAILURE);
    ATF_REQUIRE_EQ(
        model::test_result(model::test_result_broken, "Skipped test case "
                           "should have reported success but exited with "
                           "code 1"),
        engine::calculate_atf_result(utils::make_optional(body_status),
                                     fs::path("result.txt")));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__empty);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__no_newline__unknown);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__no_newline__known);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__multiline__no_newline);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__multiline__with_newline);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__unknown_status__no_reason);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__unknown_status__with_reason);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__missing_reason__no_delim);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__missing_reason__bad_delim);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__missing_reason__empty);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__broken__ok);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__broken__blanks);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__expected_death__ok);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__expected_death__blanks);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__expected_exit__ok__any);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__expected_exit__ok__specific);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__expected_exit__bad_int);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__expected_failure__ok);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__expected_failure__blanks);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__expected_signal__ok__any);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__expected_signal__ok__specific);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__expected_signal__bad_int);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__expected_timeout__ok);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__expected_timeout__blanks);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__failed__ok);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__failed__blanks);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__passed__ok);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__passed__reason);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__skipped__ok);
    ATF_ADD_TEST_CASE(tcs, atf_result__parse__skipped__blanks);

    ATF_ADD_TEST_CASE(tcs, atf_result__load__ok);
    ATF_ADD_TEST_CASE(tcs, atf_result__load__missing_file);
    ATF_ADD_TEST_CASE(tcs, atf_result__load__format_error);

    ATF_ADD_TEST_CASE(tcs, atf_result__apply__broken__ok);
    ATF_ADD_TEST_CASE(tcs, atf_result__apply__timed_out);
    ATF_ADD_TEST_CASE(tcs, atf_result__apply__expected_death__ok);
    ATF_ADD_TEST_CASE(tcs, atf_result__apply__expected_exit__ok);
    ATF_ADD_TEST_CASE(tcs, atf_result__apply__expected_exit__failed);
    ATF_ADD_TEST_CASE(tcs, atf_result__apply__expected_exit__broken);
    ATF_ADD_TEST_CASE(tcs, atf_result__apply__expected_failure__ok);
    ATF_ADD_TEST_CASE(tcs, atf_result__apply__expected_failure__broken);
    ATF_ADD_TEST_CASE(tcs, atf_result__apply__expected_signal__ok);
    ATF_ADD_TEST_CASE(tcs, atf_result__apply__expected_signal__failed);
    ATF_ADD_TEST_CASE(tcs, atf_result__apply__expected_signal__broken);
    ATF_ADD_TEST_CASE(tcs, atf_result__apply__expected_timeout__ok);
    ATF_ADD_TEST_CASE(tcs, atf_result__apply__expected_timeout__broken);
    ATF_ADD_TEST_CASE(tcs, atf_result__apply__failed__ok);
    ATF_ADD_TEST_CASE(tcs, atf_result__apply__failed__broken);
    ATF_ADD_TEST_CASE(tcs, atf_result__apply__passed__ok);
    ATF_ADD_TEST_CASE(tcs, atf_result__apply__passed__broken);
    ATF_ADD_TEST_CASE(tcs, atf_result__apply__skipped__ok);
    ATF_ADD_TEST_CASE(tcs, atf_result__apply__skipped__broken);

    ATF_ADD_TEST_CASE(tcs, atf_result__externalize__broken);
    ATF_ADD_TEST_CASE(tcs, atf_result__externalize__expected_death);
    ATF_ADD_TEST_CASE(tcs, atf_result__externalize__expected_exit);
    ATF_ADD_TEST_CASE(tcs, atf_result__externalize__expected_failure);
    ATF_ADD_TEST_CASE(tcs, atf_result__externalize__expected_signal);
    ATF_ADD_TEST_CASE(tcs, atf_result__externalize__expected_timeout);
    ATF_ADD_TEST_CASE(tcs, atf_result__externalize__failed);
    ATF_ADD_TEST_CASE(tcs, atf_result__externalize__passed);
    ATF_ADD_TEST_CASE(tcs, atf_result__externalize__skipped);

    ATF_ADD_TEST_CASE(tcs, calculate_atf_result__missing_file);
    ATF_ADD_TEST_CASE(tcs, calculate_atf_result__bad_file);
    ATF_ADD_TEST_CASE(tcs, calculate_atf_result__body_ok);
    ATF_ADD_TEST_CASE(tcs, calculate_atf_result__body_bad);
}

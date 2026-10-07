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

#include "engine/googletest_result.hpp"

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
/// \param exp_reason The expected reason describing the result, if any.
/// \param text The literal input to parse; can include multiple lines.
static void
parse_ok_test(const engine::googletest_result::types& exp_type,
              const char* exp_reason, const char* text)
{
    std::istringstream input(text);
    const engine::googletest_result actual =
        engine::googletest_result::parse(input);
    ATF_REQUIRE_EQ(exp_type, actual.type());
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
///     "googletest_result__parse__".
/// \param exp_type The expected type of the result.
/// \param exp_reason The expected reason describing the result, if any.
/// \param input The literal input to parse.
#define PARSE(name, exp_type, exp_reason, input) \
    ATF_TEST_CASE_WITHOUT_HEAD(googletest_result__parse__ ## name); \
    ATF_TEST_CASE_BODY(googletest_result__parse__ ## name) \
    { \
        parse_ok_test(exp_type, exp_reason, input); \
    }


}  // anonymous namespace


PARSE(broken,
      engine::googletest_result::broken, "invalid output",
      "invalid input");

const char disabled_context[] = (
"YOU HAVE 1 DISABLED TEST"
);

const char disabled_message[] = (
"[==========] Running 0 tests from 0 test cases.\n"
"[==========] 0 tests from 0 test cases ran. (0 ms total)\n"
"[  PASSED  ] 0 tests.\n"
"\n"
"  YOU HAVE 1 DISABLED TEST\n"
"\n"
"\n"
);

PARSE(disabled,
      engine::googletest_result::disabled, disabled_context,
      disabled_message);

const char failed_context[] = (
"pass_fail_demo.cc:8: Failure\n"
"Expected equality of these values:\n"
"  false\n"
"  true\n"
);

const char failed_message[] = (
"Note: Google Test filter = PassFailTest.Fails\n"
"[==========] Running 1 test from 1 test case.\n"
"[----------] Global test environment set-up.\n"
"[----------] 1 test from PassFailTest\n"
"[ RUN      ] PassFailTest.Fails\n"
"pass_fail_demo.cc:8: Failure\n"
"Expected equality of these values:\n"
"  false\n"
"  true\n"
"[  FAILED  ] PassFailTest.Fails (0 ms)\n"
"[----------] 1 test from PassFailTest (0 ms total)\n"
"\n"
"[----------] Global test environment tear-down\n"
"[==========] 1 test from 1 test case ran. (0 ms total)\n"
"[  PASSED  ] 0 tests.\n"
"[  FAILED  ] 1 test, listed below:\n"
"[  FAILED  ] PassFailTest.Fails\n"
"\n"
" 1 FAILED TEST\n"
);

PARSE(failed,
      engine::googletest_result::failed, failed_context,
      failed_message);

const char skipped_message[] = (
"Note: Google Test filter = SkipTest.DoesSkip\n"
"[==========] Running 1 test from 1 test suite.\n"
"[----------] Global test environment set-up.\n"
"[----------] 1 test from SkipTest\n"
"[ RUN      ] SkipTest.DoesSkip\n"
"[  SKIPPED ] SkipTest.DoesSkip (0 ms)\n"
"[----------] 1 test from SkipTest (0 ms total)\n"
"\n"
"[----------] Global test environment tear-down\n"
"[==========] 1 test from 1 test suite ran. (0 ms total)\n"
"[  PASSED  ] 0 tests.\n"
"[  SKIPPED ] 1 test, listed below:\n"
"[  SKIPPED ] SkipTest.DoesSkip\n"
);

PARSE(skipped,
      engine::googletest_result::skipped,
      engine::bogus_googletest_skipped_nul_message.c_str(),
      skipped_message
);

const char skipped_with_reason_context[] = (
"This is a reason\n"
);

const char skipped_with_reason_message[] = (
"Note: Google Test filter = SkipTest.SkipWithReason\n"
"[==========] Running 1 test from 1 test suite.\n"
"[----------] Global test environment set-up.\n"
"[----------] 1 test from SkipTest\n"
"[ RUN      ] SkipTest.SkipWithReason\n"
"This is a reason\n"
"[  SKIPPED ] SkipTest.SkipWithReason (0 ms)\n"
"[----------] 1 test from SkipTest (0 ms total)\n"
"\n"
"[----------] Global test environment tear-down\n"
"[==========] 1 test from 1 test suite ran. (0 ms total)\n"
"[  PASSED  ] 0 tests.\n"
"[  SKIPPED ] 1 test, listed below:\n"
"[  SKIPPED ] SkipTest.SkipWithReason\n"
);

PARSE(skipped_with_reason,
      engine::googletest_result::skipped, skipped_with_reason_context,
      skipped_with_reason_message);

const char successful_message[] = (
"Note: Google Test filter = PassFailTest.Passes\n"
"[==========] Running 1 test from 1 test case.\n"
"[----------] Global test environment set-up.\n"
"[----------] 1 test from PassFailTest\n"
"[ RUN      ] PassFailTest.Passes\n"
"[       OK ] PassFailTest.Passes (0 ms)\n"
"[----------] 1 test from PassFailTest (0 ms total)\n"
"\n"
"[----------] Global test environment tear-down\n"
"[==========] 1 test from 1 test case ran. (0 ms total)\n"
"[  PASSED  ] 1 test.\n"
);

PARSE(successful,
      engine::googletest_result::successful, NULL,
      successful_message);

const char successful_message2[] = (
"Note: Google Test filter = ValuesTest.ValuesWorks\n"
"[==========] Running 1 test from 1 test case.\n"
"[----------] Global test environment set-up.\n"
"[----------] 1 test from ValuesTest\n"
"[ RUN      ] ValuesTest.ValuesWorks\n"
"[       OK ] ValuesTest.ValuesWorks (0 ms)\n"
"[----------] 1 test from ValuesTest (0 ms total)\n"
"        \n"
"[----------] Global test environment tear-down\n"
"[==========] 1 test from 1 test case ran. (0 ms total)\n"
"[  PASSED  ] 1 test.\n"
);

PARSE(successful2,
      engine::googletest_result::successful, NULL,
      successful_message2);

const char successful_parameterized_message[] = (
"Note: Google Test filter = RangeZeroToFive/ParamDerivedTest/0\n"
"[==========] Running 5 tests from 1 test case.\n"
"[----------] Global test environment set-up.\n"
"[----------] 5 tests from RangeZeroToFive/ParamDerivedTest\n"
"[ RUN      ] RangeZeroToFive/ParamDerivedTest.SeesSequence/0\n"
"[       OK ] RangeZeroToFive/ParamDerivedTest.SeesSequence/0 (0 ms)\n"
"[----------] 1 test from RangeZeroToFive/ParamDerivedTest/0 (0 ms total)\n"
"\n"
"[----------] Global test environment tear-down\n"
"[==========] 1 tests from 1 test case ran. (0 ms total)\n"
"[  PASSED  ] 1 tests.\n"
);

PARSE(successful_parameterized,
      engine::googletest_result::successful, NULL,
      successful_parameterized_message);

const char successful_message_with_reason[] = (
"Note: Google Test filter = PassFailTest.PassesWithReason\n"
"[==========] Running 1 test from 1 test suite.\n"
"[----------] Global test environment set-up.\n"
"[----------] 1 test from PassFailTest\n"
"[ RUN      ] PassFailTest.PassesWithReason\n"
"This is a reason\n"
"[       OK ] PassFailTest.PassesWithReason (0 ms)\n"
"[----------] 1 test from PassFailTest (0 ms total)\n"
"\n"
"[----------] Global test environment tear-down\n"
"[==========] 1 test from 1 test suite ran. (0 ms total)\n"
"[  PASSED  ] 1 tests.\n"
);

PARSE(successful_with_reason,
      engine::googletest_result::successful, NULL,
      successful_message_with_reason);

ATF_TEST_CASE_WITHOUT_HEAD(googletest_result__load__ok);
ATF_TEST_CASE_BODY(googletest_result__load__ok)
{
    std::ofstream output("result.txt");
    ATF_REQUIRE(output);
    output << skipped_with_reason_message;
    output.close();

    const engine::googletest_result result = engine::googletest_result::load(
        utils::fs::path("result.txt"));
    ATF_REQUIRE_EQ(engine::googletest_result::skipped, result.type());
    ATF_REQUIRE(result.reason());
    ATF_REQUIRE_EQ(skipped_with_reason_context, result.reason().get());
}


ATF_TEST_CASE_WITHOUT_HEAD(googletest_result__load__missing_file);
ATF_TEST_CASE_BODY(googletest_result__load__missing_file)
{
    ATF_REQUIRE_THROW_RE(
        std::runtime_error, "Cannot open",
        engine::googletest_result::load(utils::fs::path("result.txt")));
}


ATF_TEST_CASE_WITHOUT_HEAD(googletest_result__apply__broken);
ATF_TEST_CASE_BODY(googletest_result__apply__broken)
{
    const process::status status = process::status::fake_exited(EXIT_FAILURE);
    const engine::googletest_result broken(engine::googletest_result::broken,
                                           "The reason");
    ATF_REQUIRE_EQ(broken, broken.apply(utils::make_optional(status)));
}


ATF_TEST_CASE_WITHOUT_HEAD(googletest_result__apply__disabled);
ATF_TEST_CASE_BODY(googletest_result__apply__disabled)
{
    const process::status status = process::status::fake_exited(EXIT_SUCCESS);
    const engine::googletest_result disabled(
        engine::googletest_result::disabled, "The reason");
    ATF_REQUIRE_EQ(disabled, disabled.apply(utils::make_optional(status)));
}


ATF_TEST_CASE_WITHOUT_HEAD(googletest_result__apply__failed);
ATF_TEST_CASE_BODY(googletest_result__apply__failed)
{
    const process::status status = process::status::fake_exited(EXIT_FAILURE);
    const engine::googletest_result failed(engine::googletest_result::failed,
                                           "The reason");
    ATF_REQUIRE_EQ(failed, failed.apply(utils::make_optional(status)));
}


ATF_TEST_CASE_WITHOUT_HEAD(googletest_result__apply__skipped);
ATF_TEST_CASE_BODY(googletest_result__apply__skipped)
{
    const process::status status = process::status::fake_exited(EXIT_SUCCESS);
    const engine::googletest_result skipped(engine::googletest_result::skipped,
                                            "The reason");
    ATF_REQUIRE_EQ(skipped, skipped.apply(utils::make_optional(status)));
}


ATF_TEST_CASE_WITHOUT_HEAD(googletest_result__apply__successful);
ATF_TEST_CASE_BODY(googletest_result__apply__successful)
{
    const process::status status = process::status::fake_exited(EXIT_SUCCESS);
    const engine::googletest_result
        successful(engine::googletest_result::successful);
    ATF_REQUIRE_EQ(successful, successful.apply(utils::make_optional(status)));
}


ATF_TEST_CASE_WITHOUT_HEAD(googletest_result__externalize__broken);
ATF_TEST_CASE_BODY(googletest_result__externalize__broken)
{
    const engine::googletest_result raw(engine::googletest_result::broken,
                                        "The reason");
    const model::test_result expected(model::test_result_broken,
                                      "The reason");
    ATF_REQUIRE(expected == raw.externalize());
}


ATF_TEST_CASE_WITHOUT_HEAD(googletest_result__externalize__disabled);
ATF_TEST_CASE_BODY(googletest_result__externalize__disabled)
{
    const engine::googletest_result raw(engine::googletest_result::disabled,
                                        "The reason");
    const model::test_result expected(model::test_result_skipped,
                                      "The reason");
    ATF_REQUIRE(expected == raw.externalize());
}


ATF_TEST_CASE_WITHOUT_HEAD(googletest_result__externalize__failed);
ATF_TEST_CASE_BODY(googletest_result__externalize__failed)
{
    const engine::googletest_result raw(engine::googletest_result::failed,
                                        "The reason");
    const model::test_result expected(model::test_result_failed,
                                      "The reason");
    ATF_REQUIRE(expected == raw.externalize());
}


ATF_TEST_CASE_WITHOUT_HEAD(googletest_result__externalize__skipped);
ATF_TEST_CASE_BODY(googletest_result__externalize__skipped)
{
    const engine::googletest_result raw(engine::googletest_result::skipped,
                                        "The reason");
    const model::test_result expected(model::test_result_skipped,
                                      "The reason");
    ATF_REQUIRE(expected == raw.externalize());
}


ATF_TEST_CASE_WITHOUT_HEAD(googletest_result__externalize__successful);
ATF_TEST_CASE_BODY(googletest_result__externalize__successful)
{
    const engine::googletest_result raw(engine::googletest_result::successful);
    const model::test_result expected(model::test_result_passed);
    ATF_REQUIRE_EQ(expected, raw.externalize());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, googletest_result__parse__broken);
    ATF_ADD_TEST_CASE(tcs, googletest_result__parse__disabled);
    ATF_ADD_TEST_CASE(tcs, googletest_result__parse__failed);
    ATF_ADD_TEST_CASE(tcs, googletest_result__parse__skipped);
    ATF_ADD_TEST_CASE(tcs, googletest_result__parse__skipped_with_reason);
    ATF_ADD_TEST_CASE(tcs, googletest_result__parse__successful);
    ATF_ADD_TEST_CASE(tcs, googletest_result__parse__successful2);
    ATF_ADD_TEST_CASE(tcs, googletest_result__parse__successful_parameterized);
    ATF_ADD_TEST_CASE(tcs, googletest_result__parse__successful_with_reason);

    ATF_ADD_TEST_CASE(tcs, googletest_result__load__ok);
    ATF_ADD_TEST_CASE(tcs, googletest_result__load__missing_file);

    ATF_ADD_TEST_CASE(tcs, googletest_result__apply__broken);
    ATF_ADD_TEST_CASE(tcs, googletest_result__apply__disabled);
    ATF_ADD_TEST_CASE(tcs, googletest_result__apply__failed);
    ATF_ADD_TEST_CASE(tcs, googletest_result__apply__skipped);
    ATF_ADD_TEST_CASE(tcs, googletest_result__apply__successful);

    ATF_ADD_TEST_CASE(tcs, googletest_result__externalize__broken);
    ATF_ADD_TEST_CASE(tcs, googletest_result__externalize__disabled);
    ATF_ADD_TEST_CASE(tcs, googletest_result__externalize__failed);
    ATF_ADD_TEST_CASE(tcs, googletest_result__externalize__skipped);
    ATF_ADD_TEST_CASE(tcs, googletest_result__externalize__successful);
}

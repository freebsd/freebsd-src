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

extern "C" {
#include <sys/stat.h>

#include <getopt.h>
#include <signal.h>
#include <unistd.h>

extern char** environ;
}

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

#include "utils/env.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/format/containers.ipp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/test_utils.ipp"

namespace cmdline = utils::cmdline;
namespace fs = utils::fs;

using cmdline::base_option;
using cmdline::bool_option;
using cmdline::parse;
using cmdline::parsed_cmdline;
using cmdline::string_option;

namespace fs = utils::fs;

namespace {


/// Prefix for all testcases.
const char *test_suite = "Suite.";


/// Logs an error message and exits the test with an error code.
///
/// \param str The error message to log.
static void
fail(const std::string& str)
{
    std::cerr << str << '\n';
    std::exit(EXIT_FAILURE);
}


/// A test scenario that validates the TEST_ENV_* variables.
static void
test_check_configuration_variables(void)
{
    std::set< std::string > vars;
    char** iter;
    for (iter = environ; *iter != NULL; ++iter) {
        if (std::strstr(*iter, "TEST_ENV_") == *iter) {
            vars.insert(*iter);
        }
    }

    std::set< std::string > exp_vars{
        "TEST_ENV_first=some value",
        "TEST_ENV_second=some other value"
    };

    if (vars == exp_vars) {
        std::cout <<
"Note: Google Test filter = Suite.check_configuration_variables\n"
"[==========] Running 1 test from 1 test case.\n"
"[----------] Global test environment set-up.\n"
"[----------] 1 test from Suite\n"
"[ RUN      ] Suite.check_configuration_variables\n"
"[       OK ] Suite.check_configuration_variables (0 ms)\n"
"[----------] 1 test from PassFailTest (0 ms total)\n"
"\n"
"[----------] Global test environment tear-down\n"
"[==========] 1 test from 1 test case ran. (1 ms total)\n"
"[  PASSED  ] 1 test.\n"
        ;
    } else {
        std::cout <<
"Note: Google Test filter = Suite.Fails\n"
"[==========] Running 1 test from 1 test case.\n"
"[----------] Global test environment set-up.\n"
"[----------] 1 test from PassFailTest\n"
"[ RUN      ] Suite.check_configuration_variables\n"
"pass_fail_demo.cc:12: Failure\n"
"Expected equality of these values:\n"
"  false\n"
"  true\n"
"[  FAILED  ] Suite.check_configuration_variables (0 ms)\n"
"[----------] 1 test from Suite (0 ms total)\n"
"\n"
"[----------] Global test environment tear-down\n"
"[==========] 1 test from 1 test case ran. (0 ms total)\n"
"[  PASSED  ] 0 tests.\n"
"[  FAILED  ] 1 test, listed below:\n"
"[  FAILED  ] Suite.check_configuration_variables\n"
"\n"
" 1 FAILED TEST\n"
        << F("    Expected: %s\nFound: %s\n") % exp_vars % vars;
        std::exit(EXIT_FAILURE);
    }
}


/// A test scenario that triggers a crash via abort in order to generate a
/// core dump.
static void
test_crash(void)
{
    std::abort();
}


/// A test scenario that reports some tests as failed.
static void
test_fail(void)
{
    std::cout << (
"Note: Google Test filter = Suite.fail\n"
"[==========] Running 1 test from 1 test suite.\n"
"[----------] Global test environment set-up.\n"
"[----------] 1 test from Suite\n"
"[ RUN      ] Suite.fail\n"
"gtest_macros_demo.cc:4: Failure\n"
"Failed\n"
"with a reason\n"
"[  FAILED  ] Suite.fail (0 ms)\n"
"[----------] 1 test from Suite (0 ms total)\n"
"\n"
"[----------] Global test environment tear-down\n"
"[==========] 1 test from 1 test suite ran. (0 ms total)\n"
"[  PASSED  ] 0 tests.\n"
"[  FAILED  ] 1 test, listed below:\n"
"[  FAILED  ] Suite.fail\n"
"\n"
" 1 FAILED TEST\n"
    );
    std::exit(EXIT_FAILURE);
}


/// A test scenario that passes.
static void
test_pass(void)
{
    std::cout << (
"Note: Google Test filter = Suite.pass\n"
"[==========] Running 1 test from 1 test suite.\n"
"[----------] Global test environment set-up.\n"
"[----------] 1 test from Suite\n"
"[ RUN      ] Suite.pass\n"
"[       OK ] Suite.pass (0 ms)\n"
"[----------] 1 test from Suite (0 ms total)\n"
"\n"
"[----------] Global test environment tear-down\n"
"[==========] 1 test from 1 test suite ran. (0 ms total)\n"
"[  PASSED  ] 1 test.\n"
    );
}


/// A test scenario that passes but then exits with non-zero.
static void
test_pass_but_exit_failure(void)
{
    std::cout << (
"Note: Google Test filter = Suite.pass_but_exit_failure\n"
"[==========] Running 1 test from 1 test suite.\n"
"[----------] Global test environment set-up.\n"
"[----------] 1 test from Suite\n"
"[ RUN      ] Suite.pass_but_exit_failure\n"
"[       OK ] Suite.pass_but_exit_failure (0 ms)\n"
"[----------] 1 test from Suite (0 ms total)\n"
"\n"
"[----------] Global test environment tear-down\n"
"[==========] 1 test from 1 test suite ran. (0 ms total)\n"
"[  PASSED  ] 1 test.\n"
    );

    std::exit(70);
}


/// A test scenario that times out.
///
/// Note that the timeout is defined in the Kyuafile, as the TAP interface has
/// no means for test programs to specify this by themselves.
static void
test_timeout(void)
{
    std::cout << (
"Note: Google Test filter = Suite.incomplete\n"
"[==========] Running 1 test from 1 test suite.\n"
"[----------] Global test environment set-up.\n"
"[----------] 1 test from Suite\n"
"[ RUN      ] Suite.incomplete\n"
    );

    ::sleep(10);
    const fs::path control_dir = fs::path(utils::getenv("CONTROL_DIR").get());
    std::ofstream file((control_dir / "cookie").c_str());
    if (!file)
        fail("Failed to create the control cookie");
    file.close();
}


/// Prints out program usage and exits with a non-zero exit code.
static void
usage(const char* argv0) {
    std::cout << "usage: " << argv0 << " "
              << "[--gtest_color=(auto|yes|no)] "
              << "[--gtest_filter=POSITIVE_PATTERNS] "
              << "[--gtest_list_tests]"
              << "\n\n"
              << "This program mocks a googletest test program."
              << "\n";
    std::exit(EXIT_FAILURE);
}


}  // anonymous namespace


/// Entry point to the test program.
///
/// The caller can select which test scenario to run by modifying the program's
/// basename on disk (either by a copy or by a hard link).
///
/// \todo It may be worth to split this binary into separate, smaller binaries,
/// one for every "test scenario".  We use this program as a dispatcher for
/// different "main"s, the only reason being to keep the amount of helper test
/// programs to a minimum.  However, putting this each function in its own
/// binary could simplify many other things.
///
/// \param argc The number of CLI arguments.
/// \param argv The CLI arguments themselves.  These are not used because
///     Kyua will not pass any arguments to the plain test program.
int
main(int argc, char** argv)
{
    using scenario_fn_t = void (*)(void);

    char *argv0 = argv[0];

    std::map<std::string, scenario_fn_t> scenarios{
        { "check_configuration_variables", test_check_configuration_variables },
        { "crash", test_crash },
        { "fail", test_fail },
        { "pass", test_pass },
        { "pass_but_exit_failure", test_pass_but_exit_failure },
        { "timeout", test_timeout }
    };

    const bool_option gtest_list_tests_opt(
        "gtest_list_tests", "List tests");
    const string_option gtest_color_opt(
        "gtest_color", "Enable/disable color support", "auto");
    const string_option gtest_filter_opt(
        "gtest_filter", "", "POSITIVE_PATTERNS");

    std::vector<const base_option*> options;
    /// Ignore `--gtest_color=*`.
    options.push_back(&gtest_color_opt);
    options.push_back(&gtest_filter_opt);
    options.push_back(&gtest_list_tests_opt);

    const parsed_cmdline cmdline = parse(argc, argv, options);
    INV(cmdline.arguments().empty());

    if (cmdline.has_option("gtest_list_tests")) {
        std::cout << test_suite << "\n";
        for (auto it = scenarios.begin(); it != scenarios.end(); it++) {
            std::cout << "  " << it->first << "\n";
        }
        return EXIT_SUCCESS;
    }

    INV(cmdline.has_option("gtest_filter"));

    auto gtest_filter_arg = cmdline.get_option<string_option>("gtest_filter");
    INV(gtest_filter_arg.find_first_of(test_suite) == 0);
    auto testcase = gtest_filter_arg.erase(0, strlen(test_suite));

    auto scenario = scenarios.find(testcase);
    if (scenario == scenarios.end()) {
        /// Mimic googletest test programs by printing out a usage message when
        /// a test cannot be found.
        usage(argv0);
    }

    scenario->second();

    return EXIT_SUCCESS;
}

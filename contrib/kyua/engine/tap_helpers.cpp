// Copyright 2015 The Kyua Authors.
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

#include <unistd.h>

extern char** environ;
}

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

#include "utils/env.hpp"
#include "utils/format/containers.ipp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/test_utils.ipp"

namespace fs = utils::fs;


namespace {


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

    std::set< std::string > exp_vars;
    exp_vars.insert("TEST_ENV_first=some value");
    exp_vars.insert("TEST_ENV_second=some other value");
    if (vars == exp_vars) {
        std::cout << "1..1\n"
                  << "ok 1\n";
    } else {
        std::cout << "1..1\n"
                  << "not ok 1\n"
                  << F("    Expected: %s\nFound: %s\n") % exp_vars % vars;
    }
}


/// A test scenario that crashes.
static void
test_crash(void)
{
    utils::abort_without_coredump();
}


/// A test scenario that reports some tests as failed.
static void
test_fail(void)
{
    std::cout << "1..5\n"
              << "ok 1 - This is good!\n"
              << "not ok 2\n"
              << "ok 3 - TODO Consider this as passed\n"
              << "ok 4\n"
              << "not ok 5\n";
}


/// A test scenario that passes.
static void
test_pass(void)
{
    std::cout << "1..4\n"
              << "ok 1 - This is good!\n"
              << "non-result data\n"
              << "ok 2 - SKIP Consider this as passed\n"
              << "ok 3 - TODO Consider this as passed\n"
              << "ok 4\n";
}


/// A test scenario that passes but then exits with non-zero.
static void
test_pass_but_exit_failure(void)
{
    std::cout << "1..2\n"
              << "ok 1\n"
              << "ok 2\n";
    std::exit(70);
}


/// A test scenario that times out.
///
/// Note that the timeout is defined in the Kyuafile, as the TAP interface has
/// no means for test programs to specify this by themselves.
static void
test_timeout(void)
{
    std::cout << "1..2\n"
              << "ok 1\n";

    ::sleep(10);
    const fs::path control_dir = fs::path(utils::getenv("CONTROL_DIR").get());
    std::ofstream file((control_dir / "cookie").c_str());
    if (!file)
        fail("Failed to create the control cookie");
    file.close();
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
    if (argc != 1) {
        std::cerr << "No arguments allowed; select the test scenario with the "
            "program's basename\n";
        return EXIT_FAILURE;
    }

    const std::string& test_scenario = fs::path(argv[0]).leaf_name();

    if (test_scenario == "check_configuration_variables")
        test_check_configuration_variables();
    else if (test_scenario == "crash")
        test_crash();
    else if (test_scenario == "fail")
        test_fail();
    else if (test_scenario == "pass")
        test_pass();
    else if (test_scenario == "pass_but_exit_failure")
        test_pass_but_exit_failure();
    else if (test_scenario == "timeout")
        test_timeout();
    else {
        std::cerr << "Unknown test scenario\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

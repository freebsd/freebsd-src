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

extern "C" {
#include <sys/stat.h>

#include <signal.h>
#include <unistd.h>
}

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <atf-c++.hpp>

#include "utils/env.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/operations.hpp"
#include "utils/optional.ipp"
#include "utils/test_utils.ipp"
#include "utils/text/operations.ipp"

namespace fs = utils::fs;
namespace logging = utils::logging;
namespace text = utils::text;

using utils::optional;


namespace {


/// Creates an empty file in the given directory.
///
/// \param test_case The test case currently running.
/// \param directory The name of the configuration variable that holds the path
///     to the directory in which to create the cookie file.
/// \param name The name of the cookie file to create.
static void
create_cookie(const atf::tests::tc* test_case, const char* directory,
              const char* name)
{
    if (!test_case->has_config_var(directory))
        test_case->fail(std::string(name) + " not provided");

    const fs::path control_dir(test_case->get_config_var(directory));
    std::ofstream file((control_dir / name).c_str());
    if (!file)
        test_case->fail("Failed to create the control cookie");
    file.close();
}


}  // anonymous namespace


ATF_TEST_CASE_WITH_CLEANUP(check_cleanup_workdir);
ATF_TEST_CASE_HEAD(check_cleanup_workdir)
{
    set_md_var("require.config", "control_dir");
}
ATF_TEST_CASE_BODY(check_cleanup_workdir)
{
    std::ofstream cookie("workdir_cookie");
    cookie << "1234\n";
    cookie.close();
    skip("cookie created");
}
ATF_TEST_CASE_CLEANUP(check_cleanup_workdir)
{
    const fs::path control_dir(get_config_var("control_dir"));

    std::ifstream cookie("workdir_cookie");
    if (!cookie) {
        std::ofstream((control_dir / "missing_cookie").c_str()).close();
        std::exit(EXIT_FAILURE);
    }

    std::string value;
    cookie >> value;
    if (value != "1234") {
        std::ofstream((control_dir / "invalid_cookie").c_str()).close();
        std::exit(EXIT_FAILURE);
    }

    std::ofstream((control_dir / "cookie_ok").c_str()).close();
    std::exit(EXIT_SUCCESS);
}


ATF_TEST_CASE_WITHOUT_HEAD(check_configuration_variables);
ATF_TEST_CASE_BODY(check_configuration_variables)
{
    ATF_REQUIRE(has_config_var("first"));
    ATF_REQUIRE_EQ("some value", get_config_var("first"));

    ATF_REQUIRE(has_config_var("second"));
    ATF_REQUIRE_EQ("some other value", get_config_var("second"));
}


ATF_TEST_CASE(check_list_config);
ATF_TEST_CASE_HEAD(check_list_config)
{
    std::string description = "Found:";

    if (has_config_var("var1"))
        description += " var1=" + get_config_var("var1");
    if (has_config_var("var2"))
        description += " var2=" + get_config_var("var2");

    set_md_var("descr", description);
}
ATF_TEST_CASE_BODY(check_list_config)
{
}


ATF_TEST_CASE_WITHOUT_HEAD(check_unprivileged);
ATF_TEST_CASE_BODY(check_unprivileged)
{
    if (::getuid() == 0)
        fail("Running as root, but I shouldn't be");

    std::ofstream file("cookie");
    if (!file)
        fail("Failed to create the cookie; work directory probably owned by "
             "root");
    file.close();
}


ATF_TEST_CASE_WITHOUT_HEAD(crash);
ATF_TEST_CASE_BODY(crash)
{
    std::abort();
}


ATF_TEST_CASE(crash_head);
ATF_TEST_CASE_HEAD(crash_head)
{
    utils::abort_without_coredump();
}
ATF_TEST_CASE_BODY(crash_head)
{
}


ATF_TEST_CASE_WITH_CLEANUP(crash_cleanup);
ATF_TEST_CASE_HEAD(crash_cleanup)
{
}
ATF_TEST_CASE_BODY(crash_cleanup)
{
}
ATF_TEST_CASE_CLEANUP(crash_cleanup)
{
    utils::abort_without_coredump();
}


ATF_TEST_CASE_WITHOUT_HEAD(create_cookie_in_control_dir);
ATF_TEST_CASE_BODY(create_cookie_in_control_dir)
{
    create_cookie(this, "control_dir", "cookie");
}


ATF_TEST_CASE_WITHOUT_HEAD(create_cookie_in_workdir);
ATF_TEST_CASE_BODY(create_cookie_in_workdir)
{
    std::ofstream file("cookie");
    if (!file)
        fail("Failed to create the cookie");
    file.close();
}


ATF_TEST_CASE_WITH_CLEANUP(create_cookie_from_cleanup);
ATF_TEST_CASE_HEAD(create_cookie_from_cleanup)
{
}
ATF_TEST_CASE_BODY(create_cookie_from_cleanup)
{
}
ATF_TEST_CASE_CLEANUP(create_cookie_from_cleanup)
{
    create_cookie(this, "control_dir", "cookie");
}


ATF_TEST_CASE_WITH_CLEANUP(expect_timeout);
ATF_TEST_CASE_HEAD(expect_timeout)
{
    if (has_config_var("timeout"))
        set_md_var("timeout", get_config_var("timeout"));
}
ATF_TEST_CASE_BODY(expect_timeout)
{
    expect_timeout("Times out on purpose");
    ::sleep(10);
    create_cookie(this, "control_dir", "cookie");
}
ATF_TEST_CASE_CLEANUP(expect_timeout)
{
    create_cookie(this, "control_dir", "cookie.cleanup");
}


ATF_TEST_CASE_WITH_CLEANUP(output);
ATF_TEST_CASE_HEAD(output)
{
}
ATF_TEST_CASE_BODY(output)
{
    std::cout << "Body message to stdout\n";
    std::cerr << "Body message to stderr\n";
}
ATF_TEST_CASE_CLEANUP(output)
{
    std::cout << "Cleanup message to stdout\n";
    std::cerr << "Cleanup message to stderr\n";
}


ATF_TEST_CASE(output_in_list);
ATF_TEST_CASE_HEAD(output_in_list)
{
    std::cerr << "Should not write anything!\n";
}
ATF_TEST_CASE_BODY(output_in_list)
{
}


ATF_TEST_CASE(pass);
ATF_TEST_CASE_HEAD(pass)
{
    set_md_var("descr", "Always-passing test case");
}
ATF_TEST_CASE_BODY(pass)
{
}


ATF_TEST_CASE_WITH_CLEANUP(shared_workdir);
ATF_TEST_CASE_HEAD(shared_workdir)
{
}
ATF_TEST_CASE_BODY(shared_workdir)
{
    atf::utils::create_file("shared_cookie", "");
}
ATF_TEST_CASE_CLEANUP(shared_workdir)
{
    if (!atf::utils::file_exists("shared_cookie"))
        utils::abort_without_coredump();
}


ATF_TEST_CASE(spawn_blocking_child);
ATF_TEST_CASE_HEAD(spawn_blocking_child)
{
    set_md_var("require.config", "control_dir");
}
ATF_TEST_CASE_BODY(spawn_blocking_child)
{
    pid_t pid = ::fork();
    if (pid == -1)
        fail("Cannot fork subprocess");
    else if (pid == 0) {
        for (;;)
            ::pause();
    } else {
        const fs::path name = fs::path(get_config_var("control_dir")) / "pid";
        std::ofstream pidfile(name.c_str());
        ATF_REQUIRE(pidfile);
        pidfile << pid;
        pidfile.close();
    }
}


ATF_TEST_CASE_WITH_CLEANUP(timeout_body);
ATF_TEST_CASE_HEAD(timeout_body)
{
    if (has_config_var("timeout"))
        set_md_var("timeout", get_config_var("timeout"));
}
ATF_TEST_CASE_BODY(timeout_body)
{
    ::sleep(10);
    create_cookie(this, "control_dir", "cookie");
}
ATF_TEST_CASE_CLEANUP(timeout_body)
{
    create_cookie(this, "control_dir", "cookie.cleanup");
}


ATF_TEST_CASE_WITH_CLEANUP(timeout_cleanup);
ATF_TEST_CASE_HEAD(timeout_cleanup)
{
}
ATF_TEST_CASE_BODY(timeout_cleanup)
{
}
ATF_TEST_CASE_CLEANUP(timeout_cleanup)
{
    ::sleep(10);
    create_cookie(this, "control_dir", "cookie");
}


ATF_TEST_CASE_WITHOUT_HEAD(validate_isolation);
ATF_TEST_CASE_BODY(validate_isolation)
{
    ATF_REQUIRE(utils::getenv("HOME").get() != "fake-value");
    ATF_REQUIRE(!utils::getenv("LANG"));
}


/// Wrapper around ATF_ADD_TEST_CASE to only add a test when requested.
///
/// The caller can set the TEST_CASES environment variable to a
/// whitespace-separated list of test case names to enable.  If not empty, the
/// list acts as a filter for the tests to add.
///
/// \param tcs List of test cases into which to register the test.
/// \param filters List of filters to determine whether the test applies or not.
/// \param name Name of the test case being added.
#define ADD_TEST_CASE(tcs, filters, name) \
    do { \
        if (filters.empty() || filters.find(#name) != filters.end()) \
            ATF_ADD_TEST_CASE(tcs, name); \
    } while (false)


ATF_INIT_TEST_CASES(tcs)
{
    logging::set_inmemory();

    // TODO(jmmv): Instead of using "filters", we should make TEST_CASES
    // explicitly list all the test cases to enable.  This would let us get rid
    // of some of the hacks below...
    std::set< std::string > filters;

    const optional< std::string > names_raw = utils::getenv("TEST_CASES");
    if (names_raw) {
        if (names_raw.get().empty())
            return;  // See TODO above.

        const std::vector< std::string > names = text::split(
            names_raw.get(), ' ');
        std::copy(names.begin(), names.end(),
                  std::inserter(filters, filters.begin()));
    }

    if (filters.find("crash_head") != filters.end())  // See TODO above.
        ATF_ADD_TEST_CASE(tcs, crash_head);
    if (filters.find("output_in_list") != filters.end())  // See TODO above.
        ATF_ADD_TEST_CASE(tcs, output_in_list);

    ADD_TEST_CASE(tcs, filters, check_cleanup_workdir);
    ADD_TEST_CASE(tcs, filters, check_configuration_variables);
    ADD_TEST_CASE(tcs, filters, check_list_config);
    ADD_TEST_CASE(tcs, filters, check_unprivileged);
    ADD_TEST_CASE(tcs, filters, crash);
    ADD_TEST_CASE(tcs, filters, crash_cleanup);
    ADD_TEST_CASE(tcs, filters, create_cookie_in_control_dir);
    ADD_TEST_CASE(tcs, filters, create_cookie_in_workdir);
    ADD_TEST_CASE(tcs, filters, create_cookie_from_cleanup);
    ADD_TEST_CASE(tcs, filters, expect_timeout);
    ADD_TEST_CASE(tcs, filters, output);
    ADD_TEST_CASE(tcs, filters, pass);
    ADD_TEST_CASE(tcs, filters, shared_workdir);
    ADD_TEST_CASE(tcs, filters, spawn_blocking_child);
    ADD_TEST_CASE(tcs, filters, timeout_body);
    ADD_TEST_CASE(tcs, filters, timeout_cleanup);
    ADD_TEST_CASE(tcs, filters, validate_isolation);
}

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

#include "engine/atf.hpp"

extern "C" {
#include <sys/stat.h>

#include <signal.h>
}

#include <atf-c++.hpp>

#include "engine/config.hpp"
#include "engine/scheduler.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/test_program_fwd.hpp"
#include "model/test_result.hpp"
#include "utils/config/tree.ipp"
#include "utils/datetime.hpp"
#include "utils/env.hpp"
#include "utils/format/containers.ipp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/stacktrace.hpp"
#include "utils/test_utils.ipp"

namespace config = utils::config;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace scheduler = engine::scheduler;

using utils::none;


namespace {


/// Lists the test cases associated with an ATF test program.
///
/// \param program_name Basename of the test program to run.
/// \param root Path to the base of the test suite.
/// \param names_filter Whitespace-separated list of test cases that the helper
///     test program is allowed to expose.
/// \param user_config User-provided configuration.
///
/// \return The list of loaded test cases.
static model::test_cases_map
list_one(const char* program_name,
         const fs::path& root,
         const char* names_filter = NULL,
         config::tree user_config = engine::empty_config())
{
    scheduler::scheduler_handle handle = scheduler::setup();

    const scheduler::lazy_test_program program(
        "atf", fs::path(program_name), root, "the-suite",
        model::metadata_builder().build(), user_config, handle);

    if (names_filter != NULL)
        utils::setenv("TEST_CASES", names_filter);
    const model::test_cases_map test_cases = handle.list_tests(
        &program, user_config);

    handle.cleanup();

    return test_cases;
}


/// Runs a bogus test program and checks the error result.
///
/// \param exp_error Expected error string to find.
/// \param program_name Basename of the test program to run.
/// \param root Path to the base of the test suite.
/// \param names_filter Whitespace-separated list of test cases that the helper
///     test program is allowed to expose.
static void
check_list_one_fail(const char* exp_error,
                    const char* program_name,
                    const fs::path& root,
                    const char* names_filter = NULL)
{
    const model::test_cases_map test_cases = list_one(
        program_name, root, names_filter);

    ATF_REQUIRE_EQ(1, test_cases.size());
    const model::test_case& test_case = test_cases.begin()->second;
    ATF_REQUIRE_EQ("__test_cases_list__", test_case.name());
    ATF_REQUIRE(test_case.fake_result());
    ATF_REQUIRE_MATCH(exp_error,
                      test_case.fake_result().get().reason());
}


/// Runs one ATF test program and checks its result.
///
/// \param tc Pointer to the calling test case, to obtain srcdir.
/// \param test_case_name Name of the "test case" to select from the helper
///     program.
/// \param exp_result The expected result.
/// \param user_config User-provided configuration.
/// \param check_empty_output If true, verify that the output of the test is
///     silent.  This is just a hack to implement one of the test cases; we'd
///     easily have a nicer abstraction here...
static void
run_one(const atf::tests::tc* tc, const char* test_case_name,
        const model::test_result& exp_result,
        config::tree user_config = engine::empty_config(),
        const bool check_empty_output = false)
{
    scheduler::scheduler_handle handle = scheduler::setup();

    const model::test_program_ptr program(new scheduler::lazy_test_program(
        "atf", fs::path("atf_helpers"), fs::path(tc->get_config_var("srcdir")),
        "the-suite", model::metadata_builder().build(),
        user_config, handle));

    (void)handle.spawn_test(program, test_case_name, user_config);

    scheduler::result_handle_ptr result_handle = handle.wait_any();
    const scheduler::test_result_handle* test_result_handle =
        dynamic_cast< const scheduler::test_result_handle* >(
            result_handle.get());
    atf::utils::cat_file(result_handle->stdout_file().str(), "stdout: ");
    atf::utils::cat_file(result_handle->stderr_file().str(), "stderr: ");
    ATF_REQUIRE_EQ(exp_result, test_result_handle->test_result());
    if (check_empty_output) {
        ATF_REQUIRE(atf::utils::compare_file(result_handle->stdout_file().str(),
                                             ""));
        ATF_REQUIRE(atf::utils::compare_file(result_handle->stderr_file().str(),
                                             ""));
    }
    result_handle->cleanup();
    result_handle.reset();

    handle.cleanup();
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(list__ok);
ATF_TEST_CASE_BODY(list__ok)
{
    const model::test_cases_map test_cases = list_one(
        "atf_helpers", fs::path(get_config_var("srcdir")), "pass crash");

    const model::test_cases_map exp_test_cases = model::test_cases_map_builder()
        .add("crash")
        .add("pass", model::metadata_builder()
             .set_description("Always-passing test case")
             .build())
        .build();
    ATF_REQUIRE_EQ(exp_test_cases, test_cases);
}


ATF_TEST_CASE_WITHOUT_HEAD(list__configuration_variables);
ATF_TEST_CASE_BODY(list__configuration_variables)
{
    config::tree user_config = engine::empty_config();
    user_config.set_string("test_suites.the-suite.var1", "value1");
    user_config.set_string("test_suites.the-suite.var2", "value2");

    const model::test_cases_map test_cases = list_one(
        "atf_helpers", fs::path(get_config_var("srcdir")), "check_list_config",
        user_config);

    const model::test_cases_map exp_test_cases = model::test_cases_map_builder()
        .add("check_list_config", model::metadata_builder()
             .set_description("Found: var1=value1 var2=value2")
             .build())
        .build();
    ATF_REQUIRE_EQ(exp_test_cases, test_cases);
}


ATF_TEST_CASE_WITHOUT_HEAD(list__current_directory);
ATF_TEST_CASE_BODY(list__current_directory)
{
    const fs::path helpers = fs::path(get_config_var("srcdir")) / "atf_helpers";
    ATF_REQUIRE(::symlink(helpers.c_str(), "atf_helpers") != -1);
    const model::test_cases_map test_cases = list_one(
        "atf_helpers", fs::path("."), "pass");

    const model::test_cases_map exp_test_cases = model::test_cases_map_builder()
        .add("pass", model::metadata_builder()
             .set_description("Always-passing test case")
             .build())
        .build();
    ATF_REQUIRE_EQ(exp_test_cases, test_cases);
}


ATF_TEST_CASE_WITHOUT_HEAD(list__relative_path);
ATF_TEST_CASE_BODY(list__relative_path)
{
    const fs::path helpers = fs::path(get_config_var("srcdir")) / "atf_helpers";
    ATF_REQUIRE(::mkdir("dir1", 0755) != -1);
    ATF_REQUIRE(::mkdir("dir1/dir2", 0755) != -1);
    ATF_REQUIRE(::symlink(helpers.c_str(), "dir1/dir2/atf_helpers") != -1);
    const model::test_cases_map test_cases = list_one(
        "dir2/atf_helpers", fs::path("dir1"), "pass");

    const model::test_cases_map exp_test_cases = model::test_cases_map_builder()
        .add("pass", model::metadata_builder()
             .set_description("Always-passing test case")
             .build())
        .build();
    ATF_REQUIRE_EQ(exp_test_cases, test_cases);
}


ATF_TEST_CASE_WITHOUT_HEAD(list__missing_test_program);
ATF_TEST_CASE_BODY(list__missing_test_program)
{
    check_list_one_fail("Cannot find test program", "non-existent",
                        fs::current_path());
}


ATF_TEST_CASE_WITHOUT_HEAD(list__not_a_test_program);
ATF_TEST_CASE_BODY(list__not_a_test_program)
{
    atf::utils::create_file("not-valid", "garbage\n");
    ATF_REQUIRE(::chmod("not-valid", 0755) != -1);
    check_list_one_fail("Invalid test program format", "not-valid",
                        fs::current_path());
}


ATF_TEST_CASE_WITHOUT_HEAD(list__no_permissions);
ATF_TEST_CASE_BODY(list__no_permissions)
{
    atf::utils::create_file("not-executable", "garbage\n");
    check_list_one_fail("Permission denied to run test program",
                        "not-executable", fs::current_path());
}


ATF_TEST_CASE_WITHOUT_HEAD(list__abort);
ATF_TEST_CASE_BODY(list__abort)
{
    check_list_one_fail("Test program received signal", "atf_helpers",
                        fs::path(get_config_var("srcdir")),
                        "crash_head");
}


ATF_TEST_CASE_WITHOUT_HEAD(list__empty);
ATF_TEST_CASE_BODY(list__empty)
{
    check_list_one_fail("No test cases", "atf_helpers",
                        fs::path(get_config_var("srcdir")),
                        "");
}


ATF_TEST_CASE_WITHOUT_HEAD(list__stderr_not_quiet);
ATF_TEST_CASE_BODY(list__stderr_not_quiet)
{
    check_list_one_fail("Test case list wrote to stderr", "atf_helpers",
                        fs::path(get_config_var("srcdir")),
                        "output_in_list");
}


ATF_TEST_CASE_WITHOUT_HEAD(test__body_only__passes);
ATF_TEST_CASE_BODY(test__body_only__passes)
{
    const model::test_result exp_result(model::test_result_passed);
    run_one(this, "pass", exp_result);
}


ATF_TEST_CASE_WITHOUT_HEAD(test__body_only__crashes);
ATF_TEST_CASE_BODY(test__body_only__crashes)
{
    utils::prepare_coredump_test(this);

    const model::test_result exp_result(
        model::test_result_broken,
        F("Premature exit; test case received signal %s (core dumped)") %
        SIGABRT);
    run_one(this, "crash", exp_result);
}


ATF_TEST_CASE_WITHOUT_HEAD(test__body_only__times_out);
ATF_TEST_CASE_BODY(test__body_only__times_out)
{
    config::tree user_config = engine::empty_config();
    user_config.set_string("test_suites.the-suite.control_dir",
                           fs::current_path().str());
    user_config.set_string("test_suites.the-suite.timeout", "1");

    const model::test_result exp_result(
        model::test_result_broken, "Test case body timed out");
    run_one(this, "timeout_body", exp_result, user_config);

    ATF_REQUIRE(!atf::utils::file_exists("cookie"));
}


ATF_TEST_CASE_WITHOUT_HEAD(test__body_only__configuration_variables);
ATF_TEST_CASE_BODY(test__body_only__configuration_variables)
{
    config::tree user_config = engine::empty_config();
    user_config.set_string("test_suites.the-suite.first", "some value");
    user_config.set_string("test_suites.the-suite.second", "some other value");

    const model::test_result exp_result(model::test_result_passed);
    run_one(this, "check_configuration_variables", exp_result, user_config);
}


ATF_TEST_CASE_WITHOUT_HEAD(test__body_only__no_atf_run_warning);
ATF_TEST_CASE_BODY(test__body_only__no_atf_run_warning)
{
    const model::test_result exp_result(model::test_result_passed);
    run_one(this, "pass", exp_result, engine::empty_config(), true);
}


ATF_TEST_CASE_WITHOUT_HEAD(test__body_and_cleanup__body_times_out);
ATF_TEST_CASE_BODY(test__body_and_cleanup__body_times_out)
{
    config::tree user_config = engine::empty_config();
    user_config.set_string("test_suites.the-suite.control_dir",
                           fs::current_path().str());
    user_config.set_string("test_suites.the-suite.timeout", "1");

    const model::test_result exp_result(
        model::test_result_broken, "Test case body timed out");
    run_one(this, "timeout_body", exp_result, user_config);

    ATF_REQUIRE(!atf::utils::file_exists("cookie"));
    ATF_REQUIRE(atf::utils::file_exists("cookie.cleanup"));
}


ATF_TEST_CASE_WITHOUT_HEAD(test__body_and_cleanup__cleanup_crashes);
ATF_TEST_CASE_BODY(test__body_and_cleanup__cleanup_crashes)
{
    const model::test_result exp_result(
        model::test_result_broken,
        "Test case cleanup did not terminate successfully");
    run_one(this, "crash_cleanup", exp_result);
}


ATF_TEST_CASE_WITHOUT_HEAD(test__body_and_cleanup__cleanup_times_out);
ATF_TEST_CASE_BODY(test__body_and_cleanup__cleanup_times_out)
{
    config::tree user_config = engine::empty_config();
    user_config.set_string("test_suites.the-suite.control_dir",
                           fs::current_path().str());

    scheduler::cleanup_timeout = datetime::delta(1, 0);
    const model::test_result exp_result(
        model::test_result_broken, "Test case cleanup timed out");
    run_one(this, "timeout_cleanup", exp_result, user_config);

    ATF_REQUIRE(!atf::utils::file_exists("cookie"));
}


ATF_TEST_CASE_WITHOUT_HEAD(test__body_and_cleanup__expect_timeout);
ATF_TEST_CASE_BODY(test__body_and_cleanup__expect_timeout)
{
    config::tree user_config = engine::empty_config();
    user_config.set_string("test_suites.the-suite.control_dir",
                           fs::current_path().str());
    user_config.set_string("test_suites.the-suite.timeout", "1");

    const model::test_result exp_result(
        model::test_result_expected_failure, "Times out on purpose");
    run_one(this, "expect_timeout", exp_result, user_config);

    ATF_REQUIRE(!atf::utils::file_exists("cookie"));
    ATF_REQUIRE(atf::utils::file_exists("cookie.cleanup"));
}


ATF_TEST_CASE_WITHOUT_HEAD(test__body_and_cleanup__shared_workdir);
ATF_TEST_CASE_BODY(test__body_and_cleanup__shared_workdir)
{
    const model::test_result exp_result(model::test_result_passed);
    run_one(this, "shared_workdir", exp_result);
}


ATF_INIT_TEST_CASES(tcs)
{
    scheduler::register_interface(
        "atf", std::shared_ptr< scheduler::interface >(
            new engine::atf_interface()));

    ATF_ADD_TEST_CASE(tcs, list__ok);
    ATF_ADD_TEST_CASE(tcs, list__configuration_variables);
    ATF_ADD_TEST_CASE(tcs, list__current_directory);
    ATF_ADD_TEST_CASE(tcs, list__relative_path);
    ATF_ADD_TEST_CASE(tcs, list__missing_test_program);
    ATF_ADD_TEST_CASE(tcs, list__not_a_test_program);
    ATF_ADD_TEST_CASE(tcs, list__no_permissions);
    ATF_ADD_TEST_CASE(tcs, list__abort);
    ATF_ADD_TEST_CASE(tcs, list__empty);
    ATF_ADD_TEST_CASE(tcs, list__stderr_not_quiet);

    ATF_ADD_TEST_CASE(tcs, test__body_only__passes);
    ATF_ADD_TEST_CASE(tcs, test__body_only__crashes);
    ATF_ADD_TEST_CASE(tcs, test__body_only__times_out);
    ATF_ADD_TEST_CASE(tcs, test__body_only__configuration_variables);
    ATF_ADD_TEST_CASE(tcs, test__body_only__no_atf_run_warning);
    ATF_ADD_TEST_CASE(tcs, test__body_and_cleanup__body_times_out);
    ATF_ADD_TEST_CASE(tcs, test__body_and_cleanup__cleanup_crashes);
    ATF_ADD_TEST_CASE(tcs, test__body_and_cleanup__cleanup_times_out);
    ATF_ADD_TEST_CASE(tcs, test__body_and_cleanup__expect_timeout);
    ATF_ADD_TEST_CASE(tcs, test__body_and_cleanup__shared_workdir);
}

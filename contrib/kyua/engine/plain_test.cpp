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

#include "engine/plain.hpp"

extern "C" {
#include <signal.h>
}

#include <atf-c++.hpp>

#include "engine/config.hpp"
#include "engine/scheduler.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "utils/config/tree.ipp"
#include "utils/datetime.hpp"
#include "utils/env.hpp"
#include "utils/format/containers.ipp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"

namespace config = utils::config;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace scheduler = engine::scheduler;

using utils::none;


namespace {


/// Copies the plain helper to the work directory, selecting a specific helper.
///
/// \param tc Pointer to the calling test case, to obtain srcdir.
/// \param name Name of the new binary to create.  Must match the name of a
///     valid helper, as the binary name is used to select it.
static void
copy_plain_helper(const atf::tests::tc* tc, const char* name)
{
    const fs::path srcdir(tc->get_config_var("srcdir"));
    atf::utils::copy_file((srcdir / "plain_helpers").str(), name);
}


/// Runs one plain test program and checks its result.
///
/// \param tc Pointer to the calling test case, to obtain srcdir.
/// \param test_case_name Name of the "test case" to select from the helper
///     program.
/// \param exp_result The expected result.
/// \param metadata The test case metadata.
/// \param user_config User-provided configuration variables.
static void
run_one(const atf::tests::tc* tc, const char* test_case_name,
        const model::test_result& exp_result,
        const model::metadata& metadata = model::metadata_builder().build(),
        const config::tree& user_config = engine::empty_config())
{
    copy_plain_helper(tc, test_case_name);
    const model::test_program_ptr program = model::test_program_builder(
        "plain", fs::path(test_case_name), fs::current_path(), "the-suite")
        .add_test_case("main", metadata).build_ptr();

    scheduler::scheduler_handle handle = scheduler::setup();
    (void)handle.spawn_test(program, "main", user_config);

    scheduler::result_handle_ptr result_handle = handle.wait_any();
    const scheduler::test_result_handle* test_result_handle =
        dynamic_cast< const scheduler::test_result_handle* >(
            result_handle.get());
    atf::utils::cat_file(result_handle->stdout_file().str(), "stdout: ");
    atf::utils::cat_file(result_handle->stderr_file().str(), "stderr: ");
    ATF_REQUIRE_EQ(exp_result, test_result_handle->test_result());
    result_handle->cleanup();
    result_handle.reset();

    handle.cleanup();
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(list);
ATF_TEST_CASE_BODY(list)
{
    const model::test_program program = model::test_program_builder(
        "plain", fs::path("non-existent"), fs::path("."), "unused-suite")
        .build();

    scheduler::scheduler_handle handle = scheduler::setup();
    const model::test_cases_map test_cases = handle.list_tests(
        &program, engine::empty_config());
    handle.cleanup();

    const model::test_cases_map exp_test_cases = model::test_cases_map_builder()
        .add("main").build();
    ATF_REQUIRE_EQ(exp_test_cases, test_cases);
}


ATF_TEST_CASE_WITHOUT_HEAD(test__exit_success_is_pass);
ATF_TEST_CASE_BODY(test__exit_success_is_pass)
{
    const model::test_result exp_result(model::test_result_passed);
    run_one(this, "pass", exp_result);
}


ATF_TEST_CASE_WITHOUT_HEAD(test__exit_non_zero_is_fail);
ATF_TEST_CASE_BODY(test__exit_non_zero_is_fail)
{
    const model::test_result exp_result(
        model::test_result_failed,
        "Returned non-success exit status 8");
    run_one(this, "fail", exp_result);
}


ATF_TEST_CASE_WITHOUT_HEAD(test__signal_is_broken);
ATF_TEST_CASE_BODY(test__signal_is_broken)
{
    const model::test_result exp_result(model::test_result_broken,
                                        F("Received signal %s") % SIGABRT);
    run_one(this, "crash", exp_result);
}


ATF_TEST_CASE(test__timeout_is_broken);
ATF_TEST_CASE_HEAD(test__timeout_is_broken)
{
    set_md_var("timeout", "60");
}
ATF_TEST_CASE_BODY(test__timeout_is_broken)
{
    utils::setenv("CONTROL_DIR", fs::current_path().str());

    const model::metadata metadata = model::metadata_builder()
        .set_timeout(datetime::delta(1, 0)).build();
    const model::test_result exp_result(model::test_result_broken,
                                        "Test case timed out");
    run_one(this, "timeout", exp_result, metadata);

    ATF_REQUIRE(!atf::utils::file_exists("cookie"));
}


ATF_TEST_CASE_WITHOUT_HEAD(test__configuration_variables);
ATF_TEST_CASE_BODY(test__configuration_variables)
{
    config::tree user_config = engine::empty_config();
    user_config.set_string("test_suites.a-suite.first", "unused");
    user_config.set_string("test_suites.the-suite.first", "some value");
    user_config.set_string("test_suites.the-suite.second", "some other value");
    user_config.set_string("test_suites.other-suite.first", "unused");

    const model::test_result exp_result(model::test_result_passed);
    run_one(this, "check_configuration_variables", exp_result,
            model::metadata_builder().build(), user_config);
}


ATF_INIT_TEST_CASES(tcs)
{
    scheduler::register_interface(
        "plain", std::shared_ptr< scheduler::interface >(
            new engine::plain_interface()));

    ATF_ADD_TEST_CASE(tcs, list);

    ATF_ADD_TEST_CASE(tcs, test__exit_success_is_pass);
    ATF_ADD_TEST_CASE(tcs, test__exit_non_zero_is_fail);
    ATF_ADD_TEST_CASE(tcs, test__signal_is_broken);
    ATF_ADD_TEST_CASE(tcs, test__timeout_is_broken);
    ATF_ADD_TEST_CASE(tcs, test__configuration_variables);
}

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

#include "engine/tap.hpp"

extern "C" {
#include <unistd.h>
}

#include <cstdlib>

#include "engine/exceptions.hpp"
#include "engine/execenv/execenv.hpp"
#include "engine/tap_parser.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "utils/defs.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/optional.ipp"
#include "utils/process/operations.hpp"
#include "utils/process/status.hpp"
#include "utils/sanity.hpp"

namespace config = utils::config;
namespace execenv = engine::execenv;
namespace fs = utils::fs;
namespace process = utils::process;

using utils::optional;


namespace {


/// Computes the result of a TAP test program termination.
///
/// Timeouts and bad TAP data must be handled by the caller.  Here we assume
/// that we have been able to successfully parse the TAP output.
///
/// \param summary Parsed TAP data for the test program.
/// \param status Exit status of the test program.
///
/// \return A test result.
static model::test_result
tap_to_result(const engine::tap_summary& summary,
              const process::status& status)
{
    if (summary.bailed_out()) {
        return model::test_result(model::test_result_failed, "Bailed out");
    }

    if (summary.plan() == engine::all_skipped_plan) {
        return model::test_result(model::test_result_skipped,
                                  summary.all_skipped_reason());
    }

    if (summary.not_ok_count() == 0) {
        if (status.exitstatus() == EXIT_SUCCESS) {
            return model::test_result(model::test_result_passed);
        } else {
            return model::test_result(
                model::test_result_broken,
                F("Dubious test program: reported all tests as passed "
                  "but returned exit code %s") % status.exitstatus());
        }
    } else {
        const std::size_t total = summary.ok_count() + summary.not_ok_count();
        return model::test_result(model::test_result_failed,
                                  F("%s of %s tests failed") %
                                  summary.not_ok_count() % total);
    }
}


}  // anonymous namespace


/// Executes a test program's list operation.
///
/// This method is intended to be called within a subprocess and is expected
/// to terminate execution either by exec(2)ing the test program or by
/// exiting with a failure.
void
engine::tap_interface::exec_list(
    const model::test_program& /* test_program */,
    const config::properties_map& /* vars */) const
{
    ::_exit(EXIT_SUCCESS);
}


/// Computes the test cases list of a test program.
///
/// \return A list of test cases.
model::test_cases_map
engine::tap_interface::parse_list(
    const optional< process::status >& /* status */,
    const fs::path& /* stdout_path */,
    const fs::path& /* stderr_path */) const
{
    return model::test_cases_map_builder().add("main").build();
}


/// Executes a test case of the test program.
///
/// This method is intended to be called within a subprocess and is expected
/// to terminate execution either by exec(2)ing the test program or by
/// exiting with a failure.
///
/// \param test_program The test program to execute.
/// \param test_case_name Name of the test case to invoke.
/// \param vars User-provided variables to pass to the test program.
void
engine::tap_interface::exec_test(
    const model::test_program& test_program,
    const std::string& test_case_name,
    const config::properties_map& vars,
    const fs::path& /* control_directory */) const
{
    PRE(test_case_name == "main");

    for (config::properties_map::const_iterator iter = vars.begin();
         iter != vars.end(); ++iter) {
        utils::setenv(F("TEST_ENV_%s") % (*iter).first, (*iter).second);
    }

    process::args_vector args;

    auto e = execenv::get(test_program, test_case_name);
    e->init();
    e->exec(args);
    __builtin_unreachable();
}


/// Computes the result of a test case based on its termination status.
///
/// \param status The termination status of the subprocess used to execute
///     the exec_test() method or none if the test timed out.
/// \param stdout_path Path to the file containing the stdout of the test.
///
/// \return A test result.
model::test_result
engine::tap_interface::compute_result(
    const optional< process::status >& status,
    const fs::path& /* control_directory */,
    const fs::path& stdout_path,
    const fs::path& /* stderr_path */) const
{
    if (!status) {
        return model::test_result(model::test_result_broken,
                                  "Test case timed out");
    } else {
        if (status.get().signaled()) {
            return model::test_result(
                model::test_result_broken,
                F("Received signal %s") % status.get().termsig());
        } else {
            try {
                const tap_summary summary = parse_tap_output(stdout_path);
                return tap_to_result(summary, status.get());
            } catch (const load_error& e) {
                return model::test_result(
                    model::test_result_broken,
                    F("TAP test program yielded invalid data: %s") % e.what());
            }
        }
    }
}

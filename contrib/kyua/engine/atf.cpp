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
#include <unistd.h>
}

#include <cerrno>
#include <cstdlib>
#include <fstream>

#include "engine/atf_list.hpp"
#include "engine/atf_result.hpp"
#include "engine/exceptions.hpp"
#include "engine/execenv/execenv.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "utils/defs.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/optional.ipp"
#include "utils/process/exceptions.hpp"
#include "utils/process/operations.hpp"
#include "utils/process/status.hpp"
#include "utils/stream.hpp"

namespace config = utils::config;
namespace execenv = engine::execenv;
namespace fs = utils::fs;
namespace process = utils::process;

using utils::optional;


namespace {


/// Basename of the file containing the result written by the ATF test case.
static const char* result_name = "result.atf";


/// Magic numbers returned by exec_list when exec(2) fails.
enum list_exit_code {
    exit_eacces = 90,
    exit_enoent,
    exit_enoexec,
};


}  // anonymous namespace


/// Executes a test program's list operation.
///
/// This method is intended to be called within a subprocess and is expected
/// to terminate execution either by exec(2)ing the test program or by
/// exiting with a failure.
///
/// \param test_program The test program to execute.
/// \param vars User-provided variables to pass to the test program.
void
engine::atf_interface::exec_list(const model::test_program& test_program,
                                 const config::properties_map& vars) const
{
    utils::setenv("__RUNNING_INSIDE_ATF_RUN", "internal-yes-value");

    process::args_vector args;
    for (config::properties_map::const_iterator iter = vars.begin();
         iter != vars.end(); ++iter) {
        args.push_back(F("-v%s=%s") % (*iter).first % (*iter).second);
    }

    args.push_back("-l");
    try {
        process::exec_unsafe(test_program.absolute_path(), args);
    } catch (const process::system_error& e) {
        if (e.original_errno() == EACCES)
            ::_exit(exit_eacces);
        else if (e.original_errno() == ENOENT)
            ::_exit(exit_enoent);
        else if (e.original_errno() == ENOEXEC)
            ::_exit(exit_enoexec);
        throw;
    }
}


/// Computes the test cases list of a test program.
///
/// \param status The termination status of the subprocess used to execute
///     the exec_test() method or none if the test timed out.
/// \param stdout_path Path to the file containing the stdout of the test.
/// \param stderr_path Path to the file containing the stderr of the test.
///
/// \return A list of test cases.
///
/// \throw error If there is a problem parsing the test case list.
model::test_cases_map
engine::atf_interface::parse_list(const optional< process::status >& status,
                                  const fs::path& stdout_path,
                                  const fs::path& stderr_path) const
{
    const std::string stderr_contents = utils::read_file(stderr_path);
    if (!stderr_contents.empty())
        LW("Test case list wrote to stderr: " + stderr_contents);

    if (!status)
        throw engine::error("Test case list timed out");
    if (status.get().exited()) {
        const int exitstatus = status.get().exitstatus();
        if (exitstatus == EXIT_SUCCESS) {
            // Nothing to do; fall through.
        } else if (exitstatus == exit_eacces) {
            throw engine::error("Permission denied to run test program");
        } else if (exitstatus == exit_enoent) {
            throw engine::error("Cannot find test program");
        } else if (exitstatus == exit_enoexec) {
            throw engine::error("Invalid test program format");
        } else {
            throw engine::error("Test program did not exit cleanly");
        }
    } else {
        throw engine::error("Test program received signal");
    }

    std::ifstream input(stdout_path.c_str());
    if (!input)
        throw engine::load_error(stdout_path, "Cannot open file for read");
    const model::test_cases_map test_cases = parse_atf_list(input);

    if (!stderr_contents.empty())
        throw engine::error("Test case list wrote to stderr");

    return test_cases;
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
/// \param control_directory Directory where the interface may place control
///     files.
void
engine::atf_interface::exec_test(const model::test_program& test_program,
                                 const std::string& test_case_name,
                                 const config::properties_map& vars,
                                 const fs::path& control_directory) const
{
    utils::setenv("__RUNNING_INSIDE_ATF_RUN", "internal-yes-value");

    process::args_vector args;
    for (config::properties_map::const_iterator iter = vars.begin();
         iter != vars.end(); ++iter) {
        args.push_back(F("-v%s=%s") % (*iter).first % (*iter).second);
    }

    args.push_back(F("-r%s") % (control_directory / result_name));
    args.push_back(test_case_name);

    auto e = execenv::get(test_program, test_case_name);
    e->init();
    e->exec(args);
    __builtin_unreachable();
}


/// Executes a test cleanup routine of the test program.
///
/// This method is intended to be called within a subprocess and is expected
/// to terminate execution either by exec(2)ing the test program or by
/// exiting with a failure.
///
/// \param test_program The test program to execute.
/// \param test_case_name Name of the test case to invoke.
/// \param vars User-provided variables to pass to the test program.
void
engine::atf_interface::exec_cleanup(
    const model::test_program& test_program,
    const std::string& test_case_name,
    const config::properties_map& vars,
    const fs::path& /* control_directory */) const
{
    utils::setenv("__RUNNING_INSIDE_ATF_RUN", "internal-yes-value");

    process::args_vector args;
    for (config::properties_map::const_iterator iter = vars.begin();
         iter != vars.end(); ++iter) {
        args.push_back(F("-v%s=%s") % (*iter).first % (*iter).second);
    }

    args.push_back(F("%s:cleanup") % test_case_name);

    auto e = execenv::get(test_program, test_case_name);
    e->exec(args);
    __builtin_unreachable();
}


/// Computes the result of a test case based on its termination status.
///
/// \param status The termination status of the subprocess used to execute
///     the exec_test() method or none if the test timed out.
/// \param control_directory Directory where the interface may have placed
///     control files.
///
/// \return A test result.
model::test_result
engine::atf_interface::compute_result(
    const optional< process::status >& status,
    const fs::path& control_directory,
    const fs::path& /* stdout_path */,
    const fs::path& /* stderr_path */) const
{
    return calculate_atf_result(status, control_directory / result_name);
}

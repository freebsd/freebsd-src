// Copyright 2011 The Kyua Authors.
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

#include "drivers/debug_test.hpp"

#include <stdexcept>
#include <utility>

#include "engine/filters.hpp"
#include "engine/kyuafile.hpp"
#include "engine/scanner.hpp"
#include "engine/scheduler.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/auto_cleaners.hpp"
#include "utils/optional.ipp"

namespace config = utils::config;
namespace fs = utils::fs;
namespace scheduler = engine::scheduler;

using utils::optional;


/// Executes the operation.
///
/// \param kyuafile_path The path to the Kyuafile to be loaded.
/// \param build_root If not none, path to the built test programs.
/// \param filter The test case filter to locate the test to debug.
/// \param user_config The end-user configuration properties.
/// \param stdout_path The name of the file into which to store the test case
///     stdout.
/// \param stderr_path The name of the file into which to store the test case
///     stderr.
///
/// \returns A structure with all results computed by this driver.
drivers::debug_test::result
drivers::debug_test::drive(engine::debugger_ptr debugger,
                           const fs::path& kyuafile_path,
                           const optional< fs::path > build_root,
                           const engine::test_filter& filter,
                           const config::tree& user_config,
                           const fs::path& stdout_path,
                           const fs::path& stderr_path)
{
    scheduler::scheduler_handle handle = scheduler::setup();

    const engine::kyuafile kyuafile = engine::kyuafile::load(
        kyuafile_path, build_root, user_config, handle);
    std::set< engine::test_filter > filters;
    filters.insert(filter);

    engine::scanner scanner(kyuafile.test_programs(), filters);
    optional< engine::scan_result > match;
    while (!match && !scanner.done()) {
        match = scanner.yield();
    }
    if (!match) {
        throw std::runtime_error(F("Unknown test case '%s'") % filter.str());
    } else if (!scanner.done()) {
        throw std::runtime_error(F("The filter '%s' matches more than one test "
                                 "case") % filter.str());
    }
    INV(match && scanner.done());
    const model::test_program_ptr test_program = match.get().first;
    const std::string& test_case_name = match.get().second;

    const model::test_case test_case = test_program->find(test_case_name);
    if (debugger)
        test_case.attach_debugger(debugger);

    scheduler::result_handle_ptr result_handle = handle.debug_test(
        test_program, test_case_name, user_config,
        stdout_path, stderr_path);
    const scheduler::test_result_handle* test_result_handle =
        dynamic_cast< const scheduler::test_result_handle* >(
            result_handle.get());
    const model::test_result test_result = test_result_handle->test_result();
    result_handle->cleanup();

    handle.check_interrupt();
    handle.cleanup();

    return result(engine::test_filter(
        test_program->relative_path(), test_case_name), test_result);
}

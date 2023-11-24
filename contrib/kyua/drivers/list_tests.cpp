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

#include "drivers/list_tests.hpp"

#include "engine/exceptions.hpp"
#include "engine/filters.hpp"
#include "engine/kyuafile.hpp"
#include "engine/scanner.hpp"
#include "engine/scheduler.hpp"
#include "model/test_program.hpp"
#include "utils/optional.ipp"

namespace config = utils::config;
namespace fs = utils::fs;
namespace scheduler = engine::scheduler;

using utils::optional;


/// Pure abstract destructor.
drivers::list_tests::base_hooks::~base_hooks(void)
{
}


/// Executes the operation.
///
/// \param kyuafile_path The path to the Kyuafile to be loaded.
/// \param build_root If not none, path to the built test programs.
/// \param filters The test case filters as provided by the user.
/// \param user_config The end-user configuration properties.
/// \param hooks The hooks for this execution.
///
/// \returns A structure with all results computed by this driver.
drivers::list_tests::result
drivers::list_tests::drive(const fs::path& kyuafile_path,
                           const optional< fs::path > build_root,
                           const std::set< engine::test_filter >& filters,
                           const config::tree& user_config,
                           base_hooks& hooks)
{
    scheduler::scheduler_handle handle = scheduler::setup();

    const engine::kyuafile kyuafile = engine::kyuafile::load(
        kyuafile_path, build_root, user_config, handle);

    engine::scanner scanner(kyuafile.test_programs(), filters);

    while (!scanner.done()) {
        const optional< engine::scan_result > result = scanner.yield();
        INV(result);
        hooks.got_test_case(*result.get().first, result.get().second);
    }

    handle.cleanup();

    return result(scanner.unused_filters());
}

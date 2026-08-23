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

#include "os/freebsd/prepare_kmods.hpp"

#include "cli/common.hpp"
#include "drivers/list_tests.hpp"
#include "engine/filters.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"

#include <system_error>

// FreeBSD kldload syscall
extern "C" {
#include <sys/param.h>
#include <sys/linker.h>
}


namespace {


static const std::string _name = "kmods";
static const std::string _description = "FreeBSD: load modules specified "
    "in the required_kmods metadata";


/// Hooks for list_tests to examine test cases as they come.
class list_hooks : public drivers::list_tests::base_hooks {
    /// Collected names of required kernel modules.
    std::set< std::string >& _modules;

public:
    /// Initializes the hooks.
    ///
    /// \param modules_ The set of modules to fill.
    list_hooks(std::set< std::string >& modules_) :
        _modules(modules_)
    {
    }

    /// Examine a test case as soon as it is found.
    ///
    /// \param test_program The test program containing the test case.
    /// \param test_case_name The name of the located test case.
    void
    got_test_case(const model::test_program& test_program,
                  const std::string& test_case_name)
    {
        auto test_case = test_program.find(test_case_name);
        auto kmods = test_case.get_metadata().required_kmods();
        _modules.insert(kmods.begin(), kmods.end());
    }
};


}  // anonymous namespace


namespace freebsd {


const std::string&
prepare_kmods::name() const
{
    return _name;
}


const std::string&
prepare_kmods::description() const
{
    return _description;
}


int
prepare_kmods::exec(cmdline::ui* ui, const cmdline::parsed_cmdline& cmdline,
               const config::tree& user_config) const
{
    int err = 0;

    // Collect required modules
    std::set< std::string > modules;
    list_hooks hooks(modules);
    const std::set< engine::test_filter > nofilters;
    const drivers::list_tests::result result = drivers::list_tests::drive(
        cli::kyuafile_path(cmdline), cli::build_root_path(cmdline),
        nofilters, user_config, hooks);

    // Nothing to do
    if (modules.empty())
        return 0;

    // Announce the work
    ui->out("kldload", false);
    for (auto& m : modules)
        ui->out(F(" %s") % m, false);
    ui->out("");

    if (cmdline.has_option("dry-run"))
        return 0;

    // Load the modules
    for (auto& m : modules) {
        if (::kldload(m.c_str()) != -1 || errno == EEXIST)
            continue;
        ui->out(F("WARN: cannot kldload %s") % m);
        err = EINVAL;
    }

    return err;
}


}  // namespace freebsd

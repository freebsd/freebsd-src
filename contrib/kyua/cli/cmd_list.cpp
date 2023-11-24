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

#include "cli/cmd_list.hpp"

#include <cstdlib>
#include <utility>
#include <vector>

#include "cli/common.ipp"
#include "drivers/list_tests.hpp"
#include "engine/filters.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "model/types.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/cmdline/ui.hpp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"

namespace cmdline = utils::cmdline;
namespace config = utils::config;
namespace fs = utils::fs;


namespace {


/// Hooks for list_tests to print test cases as they come.
class progress_hooks : public drivers::list_tests::base_hooks {
    /// The ui object to which to print the test cases.
    cmdline::ui* _ui;

    /// Whether to print test case details or just their names.
    bool _verbose;

public:
    /// Initializes the hooks.
    ///
    /// \param ui_ The ui object to which to print the test cases.
    /// \param verbose_ Whether to print test case details or just their names.
    progress_hooks(cmdline::ui* ui_, const bool verbose_) :
        _ui(ui_),
        _verbose(verbose_)
    {
    }

    /// Reports a test case as soon as it is found.
    ///
    /// \param test_program The test program containing the test case.
    /// \param test_case_name The name of the located test case.
    void
    got_test_case(const model::test_program& test_program,
                  const std::string& test_case_name)
    {
        cli::detail::list_test_case(_ui, _verbose, test_program,
                                    test_case_name);
    }
};


}  // anonymous namespace


/// Lists a single test case.
///
/// \param [out] ui Object to interact with the I/O of the program.
/// \param verbose Whether to be verbose or not.
/// \param test_program The test program containing the test case to print.
/// \param test_case_name The name of the test case to print.
void
cli::detail::list_test_case(cmdline::ui* ui, const bool verbose,
                            const model::test_program& test_program,
                            const std::string& test_case_name)
{
    const model::test_case& test_case = test_program.find(test_case_name);

    const std::string id = format_test_case_id(test_program, test_case_name);
    if (!verbose) {
        ui->out(id);
    } else {
        ui->out(F("%s (%s)") % id % test_program.test_suite_name());

        // TODO(jmmv): Running these for every test case is probably not the
        // fastest thing to do.
        const model::metadata default_md = model::metadata_builder().build();
        const model::properties_map default_props = default_md.to_properties();

        const model::metadata& test_md = test_case.get_metadata();
        const model::properties_map test_props = test_md.to_properties();

        for (model::properties_map::const_iterator iter = test_props.begin();
             iter != test_props.end(); iter++) {
            const model::properties_map::const_iterator default_iter =
                default_props.find((*iter).first);
            if (default_iter == default_props.end() ||
                (*iter).second != (*default_iter).second)
                ui->out(F("    %s = %s") % (*iter).first % (*iter).second);
        }
    }
}


/// Default constructor for cmd_list.
cli::cmd_list::cmd_list(void) :
    cli_command("list", "[test-program ...]", 0, -1,
                "Lists test cases and their meta-data")
{
    add_option(build_root_option);
    add_option(kyuafile_option);
    add_option(cmdline::bool_option('v', "verbose", "Show properties"));
}


/// Entry point for the "list" subcommand.
///
/// \param ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
/// \param user_config The runtime configuration of the program.
///
/// \return 0 to indicate success.
int
cli::cmd_list::run(cmdline::ui* ui, const cmdline::parsed_cmdline& cmdline,
                   const config::tree& user_config)
{
    progress_hooks hooks(ui, cmdline.has_option("verbose"));
    const drivers::list_tests::result result = drivers::list_tests::drive(
        kyuafile_path(cmdline), build_root_path(cmdline),
        parse_filters(cmdline.arguments()), user_config, hooks);

    return report_unused_filters(result.unused_filters, ui) ?
        EXIT_FAILURE : EXIT_SUCCESS;
}

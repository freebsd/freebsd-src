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

#include "cli/cmd_test.hpp"

#include <cstdlib>

#include "cli/common.ipp"
#include "drivers/run_tests.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "store/layout.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/cmdline/ui.hpp"
#include "utils/config/tree.ipp"
#include "utils/datetime.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"

namespace cmdline = utils::cmdline;
namespace config = utils::config;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace layout = store::layout;

using cli::cmd_test;


namespace {


/// Hooks to print a progress report of the execution of the tests.
class print_hooks : public drivers::run_tests::base_hooks {
    /// Object to interact with the I/O of the program.
    cmdline::ui* _ui;

    /// Whether the tests are executed in parallel or not.
    bool _parallel;

public:
    /// The amount of test results per type.
    std::map<enum model::test_result_type, unsigned long> type_count;

    /// Constructor for the hooks.
    ///
    /// \param ui_ Object to interact with the I/O of the program.
    /// \param parallel_ True if we are executing more than one test at once.
    print_hooks(cmdline::ui* ui_, const bool parallel_) :
        _ui(ui_),
        _parallel(parallel_)
    {
        for (const auto& pair : model::test_result_types)
            type_count[pair.first] = 0;
    }

    /// Called when the processing of a test case begins.
    ///
    /// \param test_program The test program containing the test case.
    /// \param test_case_name The name of the test case being executed.
    virtual void
    got_test_case(const model::test_program& test_program,
                  const std::string& test_case_name)
    {
        if (!_parallel) {
            _ui->out(F("%s  ->  ") %
                     cli::format_test_case_id(test_program, test_case_name),
                     false);
        }
    }

    /// Called when a result of a test case becomes available.
    ///
    /// \param test_program The test program containing the test case.
    /// \param test_case_name The name of the test case being executed.
    /// \param result The result of the execution of the test case.
    /// \param duration The time it took to run the test.
    virtual void
    got_result(const model::test_program& test_program,
               const std::string& test_case_name,
               const model::test_result& result,
               const datetime::delta& duration)
    {
        if (_parallel) {
            _ui->out(F("%s  ->  ") %
                     cli::format_test_case_id(test_program, test_case_name),
                     false);
        }
        _ui->out(F("%s  [%s]") % cli::format_result(result) %
            cli::format_delta(duration));

        type_count[result.type()]++;
    }
};


}  // anonymous namespace


/// Default constructor for cmd_test.
cmd_test::cmd_test(void) : cli_command(
    "test", "[test-program ...]", 0, -1, "Run tests")
{
    add_option(build_root_option);
    add_option(kyuafile_option);
    add_option(results_file_create_option);
}


/// Entry point for the "test" subcommand.
///
/// \param ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
/// \param user_config The runtime configuration of the program.
///
/// \return 0 if all tests passed, 1 otherwise.
int
cmd_test::run(cmdline::ui* ui, const cmdline::parsed_cmdline& cmdline,
              const config::tree& user_config)
{
    const layout::results_id_file_pair results = layout::new_db(
        results_file_create(cmdline), kyuafile_path(cmdline).branch_path());

    const bool parallel = (user_config.lookup< config::positive_int_node >(
                               "parallelism") > 1);

    print_hooks hooks(ui, parallel);
    const drivers::run_tests::result result = drivers::run_tests::drive(
        kyuafile_path(cmdline), build_root_path(cmdline), results.second,
        parse_filters(cmdline.arguments()), user_config, hooks);

    unsigned long total = 0;
    unsigned long good = 0;
    unsigned long bad = 0;
    for (const auto& pair : model::test_result_types) {
        const auto& type = pair.second;
        const auto count = hooks.type_count[type.id];
        total += count;
        if (type.is_run && type.is_good)
            good += count;
        if (!type.is_good)
            bad += count;
    }

    int exit_code;
    if (total > 0) {
        ui->out("");
        if (!results.first.empty()) {
            ui->out(F("Results file id is %s") % results.first);
        }
        ui->out(F("Results saved to %s") % results.second);
        ui->out("");

        ui->out(F("%s/%s passed (") % good % total, false);
        const auto& types = model::test_result_types;
        for (auto it = types.begin(); it != types.end(); it++) {
            const auto& type = it->second;
            if (!type.is_run || !type.is_good) {
                if (it != types.begin())
                    ui->out(", ", false);
                ui->out(F("%s %s") % hooks.type_count[type.id] % type.name,
                    false);
            }
        }
        ui->out(")");

        exit_code = (bad == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
    } else {
        // TODO(jmmv): Delete created empty file; it's useless!
        if (!results.first.empty()) {
            ui->out(F("Results file id is %s") % results.first);
        }
        ui->out(F("Results saved to %s") % results.second);
        exit_code = EXIT_SUCCESS;
    }

    return report_unused_filters(result.unused_filters, ui) ?
        EXIT_FAILURE : exit_code;
}

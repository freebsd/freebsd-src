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

#include "cli/cmd_debug.hpp"

#include <cstdlib>
#include <iostream>

#include "cli/common.ipp"
#include "drivers/debug_test.hpp"
#include "engine/filters.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/cmdline/ui.hpp"
#include "utils/format/macros.hpp"
#include "utils/process/executor.hpp"

namespace cmdline = utils::cmdline;
namespace config = utils::config;
namespace executor = utils::process::executor;

using cli::cmd_debug;


namespace {


const cmdline::bool_option pause_before_cleanup_upon_fail_option(
    'p',
    "pause-before-cleanup-upon-fail",
    "Pauses right before the test cleanup upon fail");


const cmdline::bool_option pause_before_cleanup_option(
    "pause-before-cleanup",
    "Pauses right before the test cleanup");


/// The debugger interface implementation.
class dbg : public engine::debugger {
    /// Object to interact with the I/O of the program.
    cmdline::ui* _ui;

    /// Representation of the command line to the subcommand.
    const cmdline::parsed_cmdline& _cmdline;

public:
    /// Constructor.
    ///
    /// \param ui_ Object to interact with the I/O of the program.
    /// \param cmdline Representation of the command line to the subcommand.
    dbg(cmdline::ui* ui, const cmdline::parsed_cmdline& cmdline) :
        _ui(ui), _cmdline(cmdline)
    {}

    void before_cleanup(
        const model::test_program_ptr&,
        const model::test_case&,
        optional< model::test_result >& result,
        executor::exit_handle& eh) const
    {
        if (_cmdline.has_option(pause_before_cleanup_upon_fail_option
            .long_name())) {
            if (result && !result.get().good()) {
                _ui->out("The test failed and paused right before its cleanup "
                    "routine.");
                _ui->out(F("Test work dir: %s") % eh.work_directory().str());
                _ui->out("Press any key to continue...");
                (void) std::cin.get();
            }
        } else if (_cmdline.has_option(pause_before_cleanup_option
            .long_name())) {
            _ui->out("The test paused right before its cleanup routine.");
            _ui->out(F("Test work dir: %s") % eh.work_directory().str());
            _ui->out("Press any key to continue...");
            (void) std::cin.get();
        }
    };

};


}  // anonymous namespace


/// Default constructor for cmd_debug.
cmd_debug::cmd_debug(void) : cli_command(
    "debug", "test_case", 1, 1,
    "Executes a single test case providing facilities for debugging")
{
    add_option(build_root_option);
    add_option(kyuafile_option);

    add_option(pause_before_cleanup_upon_fail_option);
    add_option(pause_before_cleanup_option);

    add_option(cmdline::path_option(
        "stdout", "Where to direct the standard output of the test case",
        "path", "/dev/stdout"));

    add_option(cmdline::path_option(
        "stderr", "Where to direct the standard error of the test case",
        "path", "/dev/stderr"));
}


/// Entry point for the "debug" subcommand.
///
/// \param ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
/// \param user_config The runtime debuguration of the program.
///
/// \return 0 if everything is OK, 1 if any of the necessary documents cannot be
/// opened.
int
cmd_debug::run(cmdline::ui* ui, const cmdline::parsed_cmdline& cmdline,
               const config::tree& user_config)
{
    const std::string& test_case_name = cmdline.arguments()[0];
    if (test_case_name.find(':') == std::string::npos)
        throw cmdline::usage_error(F("'%s' is not a test case identifier "
                                     "(missing ':'?)") % test_case_name);
    const engine::test_filter filter = engine::test_filter::parse(
        test_case_name);

    auto debugger = std::shared_ptr< engine::debugger >(new dbg(ui, cmdline));

    const drivers::debug_test::result result = drivers::debug_test::drive(
        debugger,
        kyuafile_path(cmdline), build_root_path(cmdline), filter, user_config,
        cmdline.get_option< cmdline::path_option >("stdout"),
        cmdline.get_option< cmdline::path_option >("stderr"));

    ui->out(F("%s  ->  %s") % cli::format_test_case_id(result.test_case) %
            cli::format_result(result.test_result));

    return result.test_result.good() ? EXIT_SUCCESS : EXIT_FAILURE;
}

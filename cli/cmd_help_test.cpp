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

#include "cli/cmd_help.hpp"

#include <algorithm>
#include <cstdlib>
#include <iterator>

#include <atf-c++.hpp>

#include "cli/common.ipp"
#include "engine/config.hpp"
#include "utils/cmdline/commands_map.ipp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/globals.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.hpp"
#include "utils/cmdline/ui_mock.hpp"
#include "utils/config/tree.ipp"
#include "utils/defs.hpp"
#include "utils/sanity.hpp"

#if defined(HAVE_CONFIG_H)
#   include "config.h"
#endif

namespace cmdline = utils::cmdline;
namespace config = utils::config;

using cli::cmd_help;


namespace {


/// Mock command with a simple definition (no options, no arguments).
///
/// Attempting to run this command will result in a crash.  It is only provided
/// to validate the generation of interactive help.
class cmd_mock_simple : public cli::cli_command {
public:
    /// Constructs a new mock command.
    ///
    /// \param name_ The name of the command to create.
    cmd_mock_simple(const char* name_) : cli::cli_command(
        name_, "", 0, 0, "Simple command")
    {
    }

    /// Runs the mock command.
    ///
    /// \return Nothing because this function is never called.
    int
    run(cmdline::ui* /* ui */,
        const cmdline::parsed_cmdline& /* cmdline */,
        const config::tree& /* user_config */)
    {
        UNREACHABLE;
    }
};


/// Mock command with a complex definition (some options, some arguments).
///
/// Attempting to run this command will result in a crash.  It is only provided
/// to validate the generation of interactive help.
class cmd_mock_complex : public cli::cli_command {
public:
    /// Constructs a new mock command.
    ///
    /// \param name_ The name of the command to create.
    cmd_mock_complex(const char* name_) : cli::cli_command(
        name_, "[arg1 .. argN]", 0, 2, "Complex command")
    {
        add_option(cmdline::bool_option("flag_a", "Flag A"));
        add_option(cmdline::bool_option('b', "flag_b", "Flag B"));
        add_option(cmdline::string_option('c', "flag_c", "Flag C", "c_arg"));
        add_option(cmdline::string_option("flag_d", "Flag D", "d_arg", "foo"));
    }

    /// Runs the mock command.
    ///
    /// \return Nothing because this function is never called.
    int
    run(cmdline::ui* /* ui */,
        const cmdline::parsed_cmdline& /* cmdline */,
        const config::tree& /* user_config */)
    {
        UNREACHABLE;
    }
};


/// Initializes the cmdline library and generates the set of test commands.
///
/// \param [out] commands A mapping that is updated to contain the commands to
///     use for testing.
static void
setup(cmdline::commands_map< cli::cli_command >& commands)
{
    cmdline::init("progname");

    commands.insert(new cmd_mock_simple("mock_simple"));
    commands.insert(new cmd_mock_complex("mock_complex"));

    commands.insert(new cmd_mock_simple("mock_simple_2"), "First");
    commands.insert(new cmd_mock_complex("mock_complex_2"), "First");

    commands.insert(new cmd_mock_simple("mock_simple_3"), "Second");
}


/// Performs a test on the global help (not that of a subcommand).
///
/// \param general_options The genral options supported by the tool, if any.
/// \param expected_options Expected lines of help output documenting the
///     options in general_options.
/// \param ui The cmdline::mock_ui object to which to write the output.
static void
global_test(const cmdline::options_vector& general_options,
            const std::vector< std::string >& expected_options,
            cmdline::ui_mock& ui)
{
    cmdline::commands_map< cli::cli_command > mock_commands;
    setup(mock_commands);

    cmdline::args_vector args;
    args.push_back("help");

    cmd_help cmd(&general_options, &mock_commands);
    ATF_REQUIRE_EQ(EXIT_SUCCESS, cmd.main(&ui, args, engine::default_config()));

    std::vector< std::string > expected;

    expected.push_back(PACKAGE " (" PACKAGE_NAME ") " PACKAGE_VERSION);
    expected.push_back("");
    expected.push_back("Usage: progname [general_options] command "
                       "[command_options] [args]");
    if (!general_options.empty()) {
        expected.push_back("");
        expected.push_back("Available general options:");
        std::copy(expected_options.begin(), expected_options.end(),
                  std::back_inserter(expected));
    }
    expected.push_back("");
    expected.push_back("Generic commands:");
    expected.push_back("  mock_complex    Complex command.");
    expected.push_back("  mock_simple     Simple command.");
    expected.push_back("");
    expected.push_back("First commands:");
    expected.push_back("  mock_complex_2  Complex command.");
    expected.push_back("  mock_simple_2   Simple command.");
    expected.push_back("");
    expected.push_back("Second commands:");
    expected.push_back("  mock_simple_3   Simple command.");
    expected.push_back("");
    expected.push_back("See kyua(1) for more details.");

    ATF_REQUIRE(expected == ui.out_log());
    ATF_REQUIRE(ui.err_log().empty());
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(global__no_options);
ATF_TEST_CASE_BODY(global__no_options)
{
    cmdline::ui_mock ui;

    cmdline::options_vector general_options;

    global_test(general_options, std::vector< std::string >(), ui);
}


ATF_TEST_CASE_WITHOUT_HEAD(global__some_options);
ATF_TEST_CASE_BODY(global__some_options)
{
    cmdline::ui_mock ui;

    cmdline::options_vector general_options;
    const cmdline::bool_option flag_a("flag_a", "Flag A");
    general_options.push_back(&flag_a);
    const cmdline::string_option flag_c('c', "lc", "Flag C", "X");
    general_options.push_back(&flag_c);

    std::vector< std::string > expected;
    expected.push_back("  --flag_a        Flag A.");
    expected.push_back("  -c X, --lc=X    Flag C.");

    global_test(general_options, expected, ui);
}


ATF_TEST_CASE_WITHOUT_HEAD(subcommand__simple);
ATF_TEST_CASE_BODY(subcommand__simple)
{
    cmdline::options_vector general_options;

    cmdline::commands_map< cli::cli_command > mock_commands;
    setup(mock_commands);

    cmdline::args_vector args;
    args.push_back("help");
    args.push_back("mock_simple");

    cmd_help cmd(&general_options, &mock_commands);
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_SUCCESS, cmd.main(&ui, args, engine::default_config()));
    ATF_REQUIRE(atf::utils::grep_collection(
        "^kyua.*" PACKAGE_VERSION, ui.out_log()));
    ATF_REQUIRE(atf::utils::grep_collection(
        "^Usage: progname \\[general_options\\] mock_simple$", ui.out_log()));
    ATF_REQUIRE(!atf::utils::grep_collection(
        "Available.*options", ui.out_log()));
    ATF_REQUIRE(atf::utils::grep_collection(
        "^See kyua-mock_simple\\(1\\) for more details.", ui.out_log()));
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(subcommand__complex);
ATF_TEST_CASE_BODY(subcommand__complex)
{
    cmdline::options_vector general_options;
    const cmdline::bool_option global_a("global_a", "Global A");
    general_options.push_back(&global_a);
    const cmdline::string_option global_c('c', "global_c", "Global C",
                                          "c_global");
    general_options.push_back(&global_c);

    cmdline::commands_map< cli::cli_command > mock_commands;
    setup(mock_commands);

    cmdline::args_vector args;
    args.push_back("help");
    args.push_back("mock_complex");

    cmd_help cmd(&general_options, &mock_commands);
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_SUCCESS, cmd.main(&ui, args, engine::default_config()));
    ATF_REQUIRE(atf::utils::grep_collection(
        "^kyua.*" PACKAGE_VERSION, ui.out_log()));
    ATF_REQUIRE(atf::utils::grep_collection(
        "^Usage: progname \\[general_options\\] mock_complex "
        "\\[command_options\\] \\[arg1 .. argN\\]$", ui.out_log()));
    ATF_REQUIRE(atf::utils::grep_collection("Available general options",
                                            ui.out_log()));
    ATF_REQUIRE(atf::utils::grep_collection("--global_a", ui.out_log()));
    ATF_REQUIRE(atf::utils::grep_collection("--global_c=c_global",
                                            ui.out_log()));
    ATF_REQUIRE(atf::utils::grep_collection("Available command options",
                                            ui.out_log()));
    ATF_REQUIRE(atf::utils::grep_collection("--flag_a   *Flag A",
                                            ui.out_log()));
    ATF_REQUIRE(atf::utils::grep_collection("-b.*--flag_b   *Flag B",
                                            ui.out_log()));
    ATF_REQUIRE(atf::utils::grep_collection(
        "-c c_arg.*--flag_c=c_arg   *Flag C", ui.out_log()));
    ATF_REQUIRE(atf::utils::grep_collection(
        "--flag_d=d_arg   *Flag D.*default.*foo", ui.out_log()));
    ATF_REQUIRE(atf::utils::grep_collection(
        "^See kyua-mock_complex\\(1\\) for more details.", ui.out_log()));
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(subcommand__unknown);
ATF_TEST_CASE_BODY(subcommand__unknown)
{
    cmdline::options_vector general_options;

    cmdline::commands_map< cli::cli_command > mock_commands;
    setup(mock_commands);

    cmdline::args_vector args;
    args.push_back("help");
    args.push_back("foobar");

    cmd_help cmd(&general_options, &mock_commands);
    cmdline::ui_mock ui;
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "command foobar.*not exist",
                         cmd.main(&ui, args, engine::default_config()));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(invalid_args);
ATF_TEST_CASE_BODY(invalid_args)
{
    cmdline::options_vector general_options;

    cmdline::commands_map< cli::cli_command > mock_commands;
    setup(mock_commands);

    cmdline::args_vector args;
    args.push_back("help");
    args.push_back("mock_simple");
    args.push_back("mock_complex");

    cmd_help cmd(&general_options, &mock_commands);
    cmdline::ui_mock ui;
    ATF_REQUIRE_THROW_RE(cmdline::usage_error, "Too many arguments",
                         cmd.main(&ui, args, engine::default_config()));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, global__no_options);
    ATF_ADD_TEST_CASE(tcs, global__some_options);
    ATF_ADD_TEST_CASE(tcs, subcommand__simple);
    ATF_ADD_TEST_CASE(tcs, subcommand__complex);
    ATF_ADD_TEST_CASE(tcs, subcommand__unknown);
    ATF_ADD_TEST_CASE(tcs, invalid_args);
}

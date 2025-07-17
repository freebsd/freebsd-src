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

#include "cli/main.hpp"

#if defined(HAVE_CONFIG_H)
#   include "config.h"
#endif

extern "C" {
#include <signal.h>
#include <unistd.h>
}

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

#include "cli/cmd_about.hpp"
#include "cli/cmd_config.hpp"
#include "cli/cmd_db_exec.hpp"
#include "cli/cmd_db_migrate.hpp"
#include "cli/cmd_debug.hpp"
#include "cli/cmd_help.hpp"
#include "cli/cmd_list.hpp"
#include "cli/cmd_report.hpp"
#include "cli/cmd_report_html.hpp"
#include "cli/cmd_report_junit.hpp"
#include "cli/cmd_test.hpp"
#include "cli/common.ipp"
#include "cli/config.hpp"
#include "engine/atf.hpp"
#include "engine/plain.hpp"
#include "engine/scheduler.hpp"
#include "engine/tap.hpp"
#include "store/exceptions.hpp"
#include "utils/cmdline/commands_map.ipp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/globals.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/cmdline/ui.hpp"
#include "utils/config/tree.ipp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/logging/operations.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"
#include "utils/signals/exceptions.hpp"

namespace cmdline = utils::cmdline;
namespace config = utils::config;
namespace fs = utils::fs;
namespace logging = utils::logging;
namespace signals = utils::signals;
namespace scheduler = engine::scheduler;

using utils::none;
using utils::optional;


namespace {


/// Registers all valid scheduler interfaces.
///
/// This is part of Kyua's setup but it is a bit strange to find it here.  I am
/// not sure what a better location would be though, so for now this is good
/// enough.
static void
register_scheduler_interfaces(void)
{
    scheduler::register_interface(
        "atf", std::shared_ptr< scheduler::interface >(
            new engine::atf_interface()));
    scheduler::register_interface(
        "plain", std::shared_ptr< scheduler::interface >(
            new engine::plain_interface()));
    scheduler::register_interface(
        "tap", std::shared_ptr< scheduler::interface >(
            new engine::tap_interface()));
}


/// Executes the given subcommand with proper usage_error reporting.
///
/// \param ui Object to interact with the I/O of the program.
/// \param command The subcommand to execute.
/// \param args The part of the command line passed to the subcommand.  The
///     first item of this collection must match the command name.
/// \param user_config The runtime configuration to pass to the subcommand.
///
/// \return The exit code of the command.  Typically 0 on success, some other
/// integer otherwise.
///
/// \throw cmdline::usage_error If the user input to the subcommand is invalid.
///     This error does not encode the command name within it, so this function
///     extends the message in the error to specify which subcommand was
///     affected.
/// \throw std::exception This propagates any uncaught exception.  Such
///     exceptions are bugs, but we let them propagate so that the runtime will
///     abort and dump core.
static int
run_subcommand(cmdline::ui* ui, cli::cli_command* command,
               const cmdline::args_vector& args,
               const config::tree& user_config)
{
    try {
        PRE(command->name() == args[0]);
        return command->main(ui, args, user_config);
    } catch (const cmdline::usage_error& e) {
        throw std::pair< std::string, cmdline::usage_error >(
            command->name(), e);
    }
}


/// Exception-safe version of main.
///
/// This function provides the real meat of the entry point of the program.  It
/// is allowed to throw some known exceptions which are parsed by the caller.
/// Doing so keeps this function simpler and allow tests to actually validate
/// that the errors reported are accurate.
///
/// \return The exit code of the program.  Should be EXIT_SUCCESS on success and
/// EXIT_FAILURE on failure.  The caller extends this to additional integers for
/// errors reported through exceptions.
///
/// \param ui Object to interact with the I/O of the program.
/// \param argc The number of arguments passed on the command line.
/// \param argv NULL-terminated array containing the command line arguments.
/// \param mock_command An extra command provided for testing purposes; should
///     just be NULL other than for tests.
///
/// \throw cmdline::usage_error If the user ran the program with invalid
///     arguments.
/// \throw std::exception This propagates any uncaught exception.  Such
///     exceptions are bugs, but we let them propagate so that the runtime will
///     abort and dump core.
static int
safe_main(cmdline::ui* ui, int argc, const char* const argv[],
          cli::cli_command_ptr mock_command)
{
    cmdline::options_vector options;
    options.push_back(&cli::config_option);
    options.push_back(&cli::variable_option);
    const cmdline::string_option loglevel_option(
        "loglevel", "Level of the messages to log", "level", "info");
    options.push_back(&loglevel_option);
    const cmdline::path_option logfile_option(
        "logfile", "Path to the log file", "file",
        cli::detail::default_log_name().c_str());
    options.push_back(&logfile_option);

    cmdline::commands_map< cli::cli_command > commands;

    commands.insert(new cli::cmd_about());
    commands.insert(new cli::cmd_config());
    commands.insert(new cli::cmd_db_exec());
    commands.insert(new cli::cmd_db_migrate());
    commands.insert(new cli::cmd_help(&options, &commands));

    commands.insert(new cli::cmd_debug(), "Workspace");
    commands.insert(new cli::cmd_list(), "Workspace");
    commands.insert(new cli::cmd_test(), "Workspace");

    commands.insert(new cli::cmd_report(), "Reporting");
    commands.insert(new cli::cmd_report_html(), "Reporting");
    commands.insert(new cli::cmd_report_junit(), "Reporting");

    if (mock_command.get() != NULL)
        commands.insert(std::move(mock_command));

    const cmdline::parsed_cmdline cmdline = cmdline::parse(argc, argv, options);

    const fs::path logfile(cmdline.get_option< cmdline::path_option >(
        "logfile"));
    fs::mkdir_p(logfile.branch_path(), 0755);
    LD(F("Log file is %s") % logfile);
    utils::install_crash_handlers(logfile.str());
    try {
        logging::set_persistency(cmdline.get_option< cmdline::string_option >(
            "loglevel"), logfile);
    } catch (const std::range_error& e) {
        throw cmdline::usage_error(e.what());
    }

    if (cmdline.arguments().empty())
        throw cmdline::usage_error("No command provided");
    const std::string cmdname = cmdline.arguments()[0];

    const config::tree user_config = cli::load_config(cmdline,
                                                      cmdname != "help");

    cli::cli_command* command = commands.find(cmdname);
    if (command == NULL)
        throw cmdline::usage_error(F("Unknown command '%s'") % cmdname);
    register_scheduler_interfaces();
    return run_subcommand(ui, command, cmdline.arguments(), user_config);
}


}  // anonymous namespace


/// Gets the name of the default log file.
///
/// \return The path to the log file.
fs::path
cli::detail::default_log_name(void)
{
    // Update doc/troubleshooting.texi if you change this algorithm.
    const optional< std::string > home(utils::getenv("HOME"));
    if (home) {
        return logging::generate_log_name(fs::path(home.get()) / ".kyua" /
                                          "logs", cmdline::progname());
    } else {
        const optional< std::string > tmpdir(utils::getenv("TMPDIR"));
        if (tmpdir) {
            return logging::generate_log_name(fs::path(tmpdir.get()),
                                              cmdline::progname());
        } else {
            return logging::generate_log_name(fs::path("/tmp"),
                                              cmdline::progname());
        }
    }
}


/// Testable entry point, with catch-all exception handlers.
///
/// This entry point does not perform any initialization of global state; it is
/// provided to allow unit-testing of the utility's entry point.
///
/// \param ui Object to interact with the I/O of the program.
/// \param argc The number of arguments passed on the command line.
/// \param argv NULL-terminated array containing the command line arguments.
/// \param mock_command An extra command provided for testing purposes; should
///     just be NULL other than for tests.
///
/// \return 0 on success, some other integer on error.
///
/// \throw std::exception This propagates any uncaught exception.  Such
///     exceptions are bugs, but we let them propagate so that the runtime will
///     abort and dump core.
int
cli::main(cmdline::ui* ui, const int argc, const char* const* const argv,
          cli_command_ptr mock_command)
{
    try {
        const int exit_code = safe_main(ui, argc, argv, std::move(mock_command));

        // Codes above 1 are reserved to report conditions captured as
        // exceptions below.
        INV(exit_code == EXIT_SUCCESS || exit_code == EXIT_FAILURE);

        return exit_code;
    } catch (const signals::interrupted_error& e) {
        cmdline::print_error(ui, F("%s.") % e.what());
        // Re-deliver the interruption signal to self so that we terminate with
        // the right status.  At this point we should NOT have any custom signal
        // handlers in place.
        ::kill(getpid(), e.signo());
        LD("Interrupt signal re-delivery did not terminate program");
        // If we reach this, something went wrong because we did not exit as
        // intended.  Return an internal error instead.  (Would be nicer to
        // abort in principle, but it wouldn't be a nice experience if it ever
        // happened.)
        return 2;
    } catch (const std::pair< std::string, cmdline::usage_error >& e) {
        const std::string message = F("Usage error for command %s: %s.") %
            e.first % e.second.what();
        LE(message);
        ui->err(message);
        ui->err(F("Type '%s help %s' for usage information.") %
                cmdline::progname() % e.first);
        return 3;
    } catch (const cmdline::usage_error& e) {
        const std::string message = F("Usage error: %s.") % e.what();
        LE(message);
        ui->err(message);
        ui->err(F("Type '%s help' for usage information.") %
                cmdline::progname());
        return 3;
    } catch (const store::old_schema_error& e) {
        const std::string message = F("The database has schema version %s, "
                                      "which is too old; please use db-migrate "
                                      "to upgrade it.") % e.old_version();
        cmdline::print_error(ui, message);
        return 2;
    } catch (const std::runtime_error& e) {
        cmdline::print_error(ui, F("%s.") % e.what());
        return 2;
    }
}


/// Delegate for ::main().
///
/// This function is supposed to be called directly from the top-level ::main()
/// function.  It takes care of initializing internal libraries and then calls
/// main(ui, argc, argv).
///
/// \pre This function can only be called once.
///
/// \throw std::exception This propagates any uncaught exception.  Such
///     exceptions are bugs, but we let them propagate so that the runtime will
///     abort and dump core.
int
cli::main(const int argc, const char* const* const argv)
{
    logging::set_inmemory();

    LI(F("%s %s") % PACKAGE % VERSION);

    std::string plain_args;
    for (const char* const* arg = argv; *arg != NULL; arg++)
        plain_args += F(" %s") % *arg;
    LI(F("Command line:%s") % plain_args);

    cmdline::init(argv[0]);
    cmdline::ui ui;

    const int exit_code = main(&ui, argc, argv);
    LI(F("Clean exit with code %s") % exit_code);
    return exit_code;
}

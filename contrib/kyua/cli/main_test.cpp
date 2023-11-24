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

extern "C" {
#include <signal.h>
}

#include <cstdlib>

#include <atf-c++.hpp>

#include "utils/cmdline/base_command.ipp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/globals.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.hpp"
#include "utils/cmdline/ui_mock.hpp"
#include "utils/datetime.hpp"
#include "utils/defs.hpp"
#include "utils/env.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/logging/operations.hpp"
#include "utils/process/child.ipp"
#include "utils/process/status.hpp"
#include "utils/test_utils.ipp"

namespace cmdline = utils::cmdline;
namespace config = utils::config;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace logging = utils::logging;
namespace process = utils::process;


namespace {


/// Fake command implementation that crashes during its execution.
class cmd_mock_crash : public cli::cli_command {
public:
    /// Constructs a new mock command.
    ///
    /// All command parameters are set to irrelevant values.
    cmd_mock_crash(void) :
        cli::cli_command("mock_error", "", 0, 0, "Mock command that crashes")
    {
    }

    /// Runs the mock command.
    ///
    /// \return Nothing because this function always aborts.
    int
    run(cmdline::ui* /* ui */,
        const cmdline::parsed_cmdline& /* cmdline */,
        const config::tree& /* user_config */)
    {
        utils::abort_without_coredump();
    }
};


/// Fake command implementation that throws an exception during its execution.
class cmd_mock_error : public cli::cli_command {
    /// Whether the command raises an exception captured by the parent or not.
    ///
    /// If this is true, the command will raise a std::runtime_error exception
    /// or a subclass of it.  The main program is in charge of capturing these
    /// and reporting them appropriately.  If false, this raises another
    /// exception that does not inherit from std::runtime_error.
    bool _unhandled;

public:
    /// Constructs a new mock command.
    ///
    /// \param unhandled If true, make run raise an exception not catched by the
    ///     main program.
    cmd_mock_error(const bool unhandled) :
        cli::cli_command("mock_error", "", 0, 0,
                         "Mock command that raises an error"),
        _unhandled(unhandled)
    {
    }

    /// Runs the mock command.
    ///
    /// \return Nothing because this function always aborts.
    ///
    /// \throw std::logic_error If _unhandled is true.
    /// \throw std::runtime_error If _unhandled is false.
    int
    run(cmdline::ui* /* ui */,
        const cmdline::parsed_cmdline& /* cmdline */,
        const config::tree& /* user_config */)
    {
        if (_unhandled)
            throw std::logic_error("This is unhandled");
        else
            throw std::runtime_error("Runtime error");
    }
};


/// Fake command implementation that prints messages during its execution.
class cmd_mock_write : public cli::cli_command {
public:
    /// Constructs a new mock command.
    ///
    /// All command parameters are set to irrelevant values.
    cmd_mock_write(void) : cli::cli_command(
        "mock_write", "", 0, 0, "Mock command that prints output")
    {
    }

    /// Runs the mock command.
    ///
    /// \param ui Object to interact with the I/O of the program.
    ///
    /// \return Nothing because this function always aborts.
    int
    run(cmdline::ui* ui,
        const cmdline::parsed_cmdline& /* cmdline */,
        const config::tree& /* user_config */)
    {
        ui->out("stdout message from subcommand");
        ui->err("stderr message from subcommand");
        return EXIT_FAILURE;
    }
};


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(detail__default_log_name__home);
ATF_TEST_CASE_BODY(detail__default_log_name__home)
{
    datetime::set_mock_now(2011, 2, 21, 21, 10, 30, 0);
    cmdline::init("progname1");

    utils::setenv("HOME", "/home//fake");
    utils::setenv("TMPDIR", "/do/not/use/this");
    ATF_REQUIRE_EQ(
        fs::path("/home/fake/.kyua/logs/progname1.20110221-211030.log"),
        cli::detail::default_log_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(detail__default_log_name__tmpdir);
ATF_TEST_CASE_BODY(detail__default_log_name__tmpdir)
{
    datetime::set_mock_now(2011, 2, 21, 21, 10, 50, 987);
    cmdline::init("progname2");

    utils::unsetenv("HOME");
    utils::setenv("TMPDIR", "/a/b//c");
    ATF_REQUIRE_EQ(fs::path("/a/b/c/progname2.20110221-211050.log"),
                   cli::detail::default_log_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(detail__default_log_name__hardcoded);
ATF_TEST_CASE_BODY(detail__default_log_name__hardcoded)
{
    datetime::set_mock_now(2011, 2, 21, 21, 15, 00, 123456);
    cmdline::init("progname3");

    utils::unsetenv("HOME");
    utils::unsetenv("TMPDIR");
    ATF_REQUIRE_EQ(fs::path("/tmp/progname3.20110221-211500.log"),
                   cli::detail::default_log_name());
}


ATF_TEST_CASE_WITHOUT_HEAD(main__no_args);
ATF_TEST_CASE_BODY(main__no_args)
{
    logging::set_inmemory();
    cmdline::init("progname");

    const int argc = 1;
    const char* const argv[] = {"progname", NULL};

    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(3, cli::main(&ui, argc, argv));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(atf::utils::grep_collection("Usage error: No command provided",
                                            ui.err_log()));
    ATF_REQUIRE(atf::utils::grep_collection("Type.*progname help",
                                            ui.err_log()));
}


ATF_TEST_CASE_WITHOUT_HEAD(main__unknown_command);
ATF_TEST_CASE_BODY(main__unknown_command)
{
    logging::set_inmemory();
    cmdline::init("progname");

    const int argc = 2;
    const char* const argv[] = {"progname", "foo", NULL};

    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(3, cli::main(&ui, argc, argv));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(atf::utils::grep_collection("Usage error: Unknown command.*foo",
                                            ui.err_log()));
    ATF_REQUIRE(atf::utils::grep_collection("Type.*progname help",
                                            ui.err_log()));
}


ATF_TEST_CASE_WITHOUT_HEAD(main__logfile__default);
ATF_TEST_CASE_BODY(main__logfile__default)
{
    logging::set_inmemory();
    datetime::set_mock_now(2011, 2, 21, 21, 30, 00, 0);
    cmdline::init("progname");

    const int argc = 1;
    const char* const argv[] = {"progname", NULL};

    cmdline::ui_mock ui;
    ATF_REQUIRE(!fs::exists(fs::path(
        ".kyua/logs/progname.20110221-213000.log")));
    ATF_REQUIRE_EQ(3, cli::main(&ui, argc, argv));
    ATF_REQUIRE(fs::exists(fs::path(
        ".kyua/logs/progname.20110221-213000.log")));
}


ATF_TEST_CASE_WITHOUT_HEAD(main__logfile__override);
ATF_TEST_CASE_BODY(main__logfile__override)
{
    logging::set_inmemory();
    datetime::set_mock_now(2011, 2, 21, 21, 30, 00, 321);
    cmdline::init("progname");

    const int argc = 2;
    const char* const argv[] = {"progname", "--logfile=test.log", NULL};

    cmdline::ui_mock ui;
    ATF_REQUIRE(!fs::exists(fs::path("test.log")));
    ATF_REQUIRE_EQ(3, cli::main(&ui, argc, argv));
    ATF_REQUIRE(!fs::exists(fs::path(
        ".kyua/logs/progname.20110221-213000.log")));
    ATF_REQUIRE(fs::exists(fs::path("test.log")));
}


ATF_TEST_CASE_WITHOUT_HEAD(main__loglevel__default);
ATF_TEST_CASE_BODY(main__loglevel__default)
{
    logging::set_inmemory();
    cmdline::init("progname");

    const int argc = 2;
    const char* const argv[] = {"progname", "--logfile=test.log", NULL};

    LD("Mock debug message");
    LE("Mock error message");
    LI("Mock info message");
    LW("Mock warning message");

    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(3, cli::main(&ui, argc, argv));
    ATF_REQUIRE(!atf::utils::grep_file("Mock debug message", "test.log"));
    ATF_REQUIRE(atf::utils::grep_file("Mock error message", "test.log"));
    ATF_REQUIRE(atf::utils::grep_file("Mock info message", "test.log"));
    ATF_REQUIRE(atf::utils::grep_file("Mock warning message", "test.log"));
}


ATF_TEST_CASE_WITHOUT_HEAD(main__loglevel__higher);
ATF_TEST_CASE_BODY(main__loglevel__higher)
{
    logging::set_inmemory();
    cmdline::init("progname");

    const int argc = 3;
    const char* const argv[] = {"progname", "--logfile=test.log",
                                "--loglevel=debug", NULL};

    LD("Mock debug message");
    LE("Mock error message");
    LI("Mock info message");
    LW("Mock warning message");

    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(3, cli::main(&ui, argc, argv));
    ATF_REQUIRE(atf::utils::grep_file("Mock debug message", "test.log"));
    ATF_REQUIRE(atf::utils::grep_file("Mock error message", "test.log"));
    ATF_REQUIRE(atf::utils::grep_file("Mock info message", "test.log"));
    ATF_REQUIRE(atf::utils::grep_file("Mock warning message", "test.log"));
}


ATF_TEST_CASE_WITHOUT_HEAD(main__loglevel__lower);
ATF_TEST_CASE_BODY(main__loglevel__lower)
{
    logging::set_inmemory();
    cmdline::init("progname");

    const int argc = 3;
    const char* const argv[] = {"progname", "--logfile=test.log",
                                "--loglevel=warning", NULL};

    LD("Mock debug message");
    LE("Mock error message");
    LI("Mock info message");
    LW("Mock warning message");

    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(3, cli::main(&ui, argc, argv));
    ATF_REQUIRE(!atf::utils::grep_file("Mock debug message", "test.log"));
    ATF_REQUIRE(atf::utils::grep_file("Mock error message", "test.log"));
    ATF_REQUIRE(!atf::utils::grep_file("Mock info message", "test.log"));
    ATF_REQUIRE(atf::utils::grep_file("Mock warning message", "test.log"));
}


ATF_TEST_CASE_WITHOUT_HEAD(main__loglevel__error);
ATF_TEST_CASE_BODY(main__loglevel__error)
{
    logging::set_inmemory();
    cmdline::init("progname");

    const int argc = 3;
    const char* const argv[] = {"progname", "--logfile=test.log",
                                "--loglevel=i-am-invalid", NULL};

    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(3, cli::main(&ui, argc, argv));
    ATF_REQUIRE(atf::utils::grep_collection("Usage error.*i-am-invalid",
                                            ui.err_log()));
    ATF_REQUIRE(!fs::exists(fs::path("test.log")));
}


ATF_TEST_CASE_WITHOUT_HEAD(main__subcommand__ok);
ATF_TEST_CASE_BODY(main__subcommand__ok)
{
    logging::set_inmemory();
    cmdline::init("progname");

    const int argc = 2;
    const char* const argv[] = {"progname", "mock_write", NULL};

    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_FAILURE,
                   cli::main(&ui, argc, argv,
                             cli::cli_command_ptr(new cmd_mock_write())));
    ATF_REQUIRE_EQ(1, ui.out_log().size());
    ATF_REQUIRE_EQ("stdout message from subcommand", ui.out_log()[0]);
    ATF_REQUIRE_EQ(1, ui.err_log().size());
    ATF_REQUIRE_EQ("stderr message from subcommand", ui.err_log()[0]);
}


ATF_TEST_CASE_WITHOUT_HEAD(main__subcommand__invalid_args);
ATF_TEST_CASE_BODY(main__subcommand__invalid_args)
{
    logging::set_inmemory();
    cmdline::init("progname");

    const int argc = 3;
    const char* const argv[] = {"progname", "mock_write", "bar", NULL};

    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(3,
                   cli::main(&ui, argc, argv,
                             cli::cli_command_ptr(new cmd_mock_write())));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(atf::utils::grep_collection(
        "Usage error for command mock_write: Too many arguments.",
        ui.err_log()));
    ATF_REQUIRE(atf::utils::grep_collection("Type.*progname help",
                                            ui.err_log()));
}


ATF_TEST_CASE_WITHOUT_HEAD(main__subcommand__runtime_error);
ATF_TEST_CASE_BODY(main__subcommand__runtime_error)
{
    logging::set_inmemory();
    cmdline::init("progname");

    const int argc = 2;
    const char* const argv[] = {"progname", "mock_error", NULL};

    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(2, cli::main(&ui, argc, argv,
        cli::cli_command_ptr(new cmd_mock_error(false))));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(atf::utils::grep_collection("progname: E: Runtime error.",
                                            ui.err_log()));
}


ATF_TEST_CASE_WITHOUT_HEAD(main__subcommand__unhandled_exception);
ATF_TEST_CASE_BODY(main__subcommand__unhandled_exception)
{
    logging::set_inmemory();
    cmdline::init("progname");

    const int argc = 2;
    const char* const argv[] = {"progname", "mock_error", NULL};

    cmdline::ui_mock ui;
    ATF_REQUIRE_THROW(std::logic_error, cli::main(&ui, argc, argv,
        cli::cli_command_ptr(new cmd_mock_error(true))));
}


static void
do_subcommand_crash(void)
{
    logging::set_inmemory();
    cmdline::init("progname");

    const int argc = 2;
    const char* const argv[] = {"progname", "mock_error", NULL};

    cmdline::ui_mock ui;
    cli::main(&ui, argc, argv,
              cli::cli_command_ptr(new cmd_mock_crash()));
}


ATF_TEST_CASE_WITHOUT_HEAD(main__subcommand__crash);
ATF_TEST_CASE_BODY(main__subcommand__crash)
{
    const process::status status = process::child::fork_files(
        do_subcommand_crash, fs::path("stdout.txt"),
        fs::path("stderr.txt"))->wait();
    ATF_REQUIRE(status.signaled());
    ATF_REQUIRE_EQ(SIGABRT, status.termsig());
    ATF_REQUIRE(atf::utils::grep_file("Fatal signal", "stderr.txt"));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, detail__default_log_name__home);
    ATF_ADD_TEST_CASE(tcs, detail__default_log_name__tmpdir);
    ATF_ADD_TEST_CASE(tcs, detail__default_log_name__hardcoded);

    ATF_ADD_TEST_CASE(tcs, main__no_args);
    ATF_ADD_TEST_CASE(tcs, main__unknown_command);
    ATF_ADD_TEST_CASE(tcs, main__logfile__default);
    ATF_ADD_TEST_CASE(tcs, main__logfile__override);
    ATF_ADD_TEST_CASE(tcs, main__loglevel__default);
    ATF_ADD_TEST_CASE(tcs, main__loglevel__higher);
    ATF_ADD_TEST_CASE(tcs, main__loglevel__lower);
    ATF_ADD_TEST_CASE(tcs, main__loglevel__error);
    ATF_ADD_TEST_CASE(tcs, main__subcommand__ok);
    ATF_ADD_TEST_CASE(tcs, main__subcommand__invalid_args);
    ATF_ADD_TEST_CASE(tcs, main__subcommand__runtime_error);
    ATF_ADD_TEST_CASE(tcs, main__subcommand__unhandled_exception);
    ATF_ADD_TEST_CASE(tcs, main__subcommand__crash);
}

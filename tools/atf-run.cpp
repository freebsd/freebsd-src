//
// Automated Testing Framework (atf)
//
// Copyright (c) 2007 The NetBSD Foundation, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
// CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
// IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#if defined(HAVE_CONFIG_H)
#include "bconfig.h"
#endif

extern "C" {
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
}

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#include "application.hpp"
#include "atffile.hpp"
#include "config.hpp"
#include "config_file.hpp"
#include "env.hpp"
#include "exceptions.hpp"
#include "fs.hpp"
#include "parser.hpp"
#include "process.hpp"
#include "requirements.hpp"
#include "test-program.hpp"
#include "text.hpp"

namespace {

typedef std::map< std::string, std::string > vars_map;

} // anonymous namespace

#if defined(MAXCOMLEN)
static const std::string::size_type max_core_name_length = MAXCOMLEN;
#else
static const std::string::size_type max_core_name_length = std::string::npos;
#endif

class atf_run : public tools::application::app {
    static const char* m_description;

    vars_map m_cmdline_vars;

    static vars_map::value_type parse_var(const std::string&);

    void process_option(int, const char*);
    std::string specific_args(void) const;
    options_set specific_options(void) const;

    void parse_vflag(const std::string&);

    std::vector< std::string > conf_args(void) const;

    size_t count_tps(std::vector< std::string >) const;

    int run_test(const tools::fs::path&, tools::test_program::atf_tps_writer&,
                 const vars_map&);
    int run_test_directory(const tools::fs::path&,
                           tools::test_program::atf_tps_writer&);
    int run_test_program(const tools::fs::path&,
                         tools::test_program::atf_tps_writer&,
                         const vars_map&);

    tools::test_program::test_case_result get_test_case_result(
        const std::string&, const tools::process::status&,
        const tools::fs::path&) const;

public:
    atf_run(void);

    int main(void);
};

static void
sanitize_gdb_env(void)
{
    try {
        tools::env::unset("TERM");
    } catch (...) {
        // Just swallow exceptions here; they cannot propagate into C, which
        // is where this function is called from, and even if these exceptions
        // appear they are benign.
    }
}

static void
dump_stacktrace(const tools::fs::path& tp, const tools::process::status& s,
                const tools::fs::path& workdir,
                tools::test_program::atf_tps_writer& w)
{
    assert(s.signaled() && s.coredump());

    w.stderr_tc("Test program crashed; attempting to get stack trace");

    const tools::fs::path corename = workdir /
        (tp.leaf_name().substr(0, max_core_name_length) + ".core");
    if (!tools::fs::exists(corename)) {
        w.stderr_tc("Expected file " + corename.str() + " not found");
        return;
    }

    const tools::fs::path gdb(GDB);
    const tools::fs::path gdbout = workdir / "gdb.out";
    const tools::process::argv_array args(gdb.leaf_name().c_str(), "-batch",
                                        "-q", "-ex", "bt", tp.c_str(),
                                        corename.c_str(), NULL);
    tools::process::status status = tools::process::exec(
        gdb, args,
        tools::process::stream_redirect_path(gdbout),
        tools::process::stream_redirect_path(tools::fs::path("/dev/null")),
        sanitize_gdb_env);
    if (!status.exited() || status.exitstatus() != EXIT_SUCCESS) {
        w.stderr_tc("Execution of " GDB " failed");
        return;
    }

    std::ifstream input(gdbout.c_str());
    if (input) {
        std::string line;
        while (std::getline(input, line).good())
            w.stderr_tc(line);
        input.close();
    }

    w.stderr_tc("Stack trace complete");
}

const char* atf_run::m_description =
    "atf-run is a tool that runs tests programs and collects their "
    "results.";

atf_run::atf_run(void) :
    app(m_description, "atf-run(1)", "atf(7)")
{
}

void
atf_run::process_option(int ch, const char* arg)
{
    switch (ch) {
    case 'v':
        parse_vflag(arg);
        break;

    default:
        std::abort();
    }
}

std::string
atf_run::specific_args(void)
    const
{
    return "[test-program1 .. test-programN]";
}

atf_run::options_set
atf_run::specific_options(void)
    const
{
    using tools::application::option;
    options_set opts;
    opts.insert(option('v', "var=value", "Sets the configuration variable "
                                         "`var' to `value'; overrides "
                                         "values in configuration files"));
    return opts;
}

void
atf_run::parse_vflag(const std::string& str)
{
    if (str.empty())
        throw std::runtime_error("-v requires a non-empty argument");

    std::vector< std::string > ws = tools::text::split(str, "=");
    if (ws.size() == 1 && str[str.length() - 1] == '=') {
        m_cmdline_vars[ws[0]] = "";
    } else {
        if (ws.size() != 2)
            throw std::runtime_error("-v requires an argument of the form "
                                     "var=value");

        m_cmdline_vars[ws[0]] = ws[1];
    }
}

int
atf_run::run_test(const tools::fs::path& tp,
                  tools::test_program::atf_tps_writer& w,
                  const vars_map& config)
{
    tools::fs::file_info fi(tp);

    int errcode;
    if (fi.get_type() == tools::fs::file_info::dir_type)
        errcode = run_test_directory(tp, w);
    else {
        const vars_map effective_config =
            tools::config_file::merge_configs(config, m_cmdline_vars);

        errcode = run_test_program(tp, w, effective_config);
    }
    return errcode;
}

int
atf_run::run_test_directory(const tools::fs::path& tp,
                            tools::test_program::atf_tps_writer& w)
{
    tools::atffile af = tools::read_atffile(tp / "Atffile");

    vars_map test_suite_vars;
    {
        vars_map::const_iterator iter = af.props().find("test-suite");
        assert(iter != af.props().end());
        test_suite_vars = tools::config_file::read_config_files((*iter).second);
    }

    bool ok = true;
    for (std::vector< std::string >::const_iterator iter = af.tps().begin();
         iter != af.tps().end(); iter++) {
        const bool result = run_test(tp / *iter, w,
            tools::config_file::merge_configs(af.conf(), test_suite_vars));
        ok &= (result == EXIT_SUCCESS);
    }

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

tools::test_program::test_case_result
atf_run::get_test_case_result(const std::string& broken_reason,
                              const tools::process::status& s,
                              const tools::fs::path& resfile)
    const
{
    using tools::text::to_string;
    using tools::test_program::read_test_case_result;
    using tools::test_program::test_case_result;

    if (!broken_reason.empty()) {
        test_case_result tcr;

        try {
            tcr = read_test_case_result(resfile);

            if (tcr.state() == "expected_timeout") {
                return tcr;
            } else {
                return test_case_result("failed", -1, broken_reason);
            }
        } catch (const std::runtime_error&) {
            return test_case_result("failed", -1, broken_reason);
        }
    }

    if (s.exited()) {
        test_case_result tcr;

        try {
            tcr = read_test_case_result(resfile);
        } catch (const std::runtime_error& e) {
            return test_case_result("failed", -1, "Test case exited "
                "normally but failed to create the results file: " +
                std::string(e.what()));
        }

        if (tcr.state() == "expected_death") {
            return tcr;
        } else if (tcr.state() == "expected_exit") {
            if (tcr.value() == -1 || s.exitstatus() == tcr.value())
                return tcr;
            else
                return test_case_result("failed", -1, "Test case was "
                    "expected to exit with a " + to_string(tcr.value()) +
                    " error code but returned " + to_string(s.exitstatus()));
        } else if (tcr.state() == "expected_failure") {
            if (s.exitstatus() == EXIT_SUCCESS)
                return tcr;
            else
                return test_case_result("failed", -1, "Test case returned an "
                    "error in expected_failure mode but it should not have");
        } else if (tcr.state() == "expected_signal") {
            return test_case_result("failed", -1, "Test case exited cleanly "
                "but was expected to receive a signal");
        } else if (tcr.state() == "failed") {
            if (s.exitstatus() == EXIT_SUCCESS)
                return test_case_result("failed", -1, "Test case "
                    "exited successfully but reported failure");
            else
                return tcr;
        } else if (tcr.state() == "passed") {
            if (s.exitstatus() == EXIT_SUCCESS)
                return tcr;
            else
                return test_case_result("failed", -1, "Test case exited as "
                    "passed but reported an error");
        } else if (tcr.state() == "skipped") {
            if (s.exitstatus() == EXIT_SUCCESS)
                return tcr;
            else
                return test_case_result("failed", -1, "Test case exited as "
                    "skipped but reported an error");
        }
    } else if (s.signaled()) {
        test_case_result tcr;

        try {
            tcr = read_test_case_result(resfile);
        } catch (const std::runtime_error&) {
            return test_case_result("failed", -1, "Test program received "
                "signal " + tools::text::to_string(s.termsig()) +
                (s.coredump() ? " (core dumped)" : ""));
        }

        if (tcr.state() == "expected_death") {
            return tcr;
        } else if (tcr.state() == "expected_signal") {
            if (tcr.value() == -1 || s.termsig() == tcr.value())
                return tcr;
            else
                return test_case_result("failed", -1, "Test case was "
                    "expected to exit due to a " + to_string(tcr.value()) +
                    " signal but got " + to_string(s.termsig()));
        } else {
            return test_case_result("failed", -1, "Test program received "
                "signal " + tools::text::to_string(s.termsig()) +
                (s.coredump() ? " (core dumped)" : "") + " and created a "
                "bogus results file");
        }
    }
    std::abort();
    return test_case_result();
}

int
atf_run::run_test_program(const tools::fs::path& tp,
                          tools::test_program::atf_tps_writer& w,
                          const vars_map& config)
{
    int errcode = EXIT_SUCCESS;

    tools::test_program::metadata md;
    try {
        md = tools::test_program::get_metadata(tp, config);
    } catch (const tools::parser::format_error& e) {
        w.start_tp(tp.str(), 0);
        w.end_tp("Invalid format for test case list: " + std::string(e.what()));
        return EXIT_FAILURE;
    } catch (const tools::parser::parse_errors& e) {
        const std::string reason = tools::text::join(e, "; ");
        w.start_tp(tp.str(), 0);
        w.end_tp("Invalid format for test case list: " + reason);
        return EXIT_FAILURE;
    }

    tools::fs::temp_dir resdir(
        tools::fs::path(tools::config::get("atf_workdir")) / "atf-run.XXXXXX");

    w.start_tp(tp.str(), md.test_cases.size());
    if (md.test_cases.empty()) {
        w.end_tp("Bogus test program: reported 0 test cases");
        errcode = EXIT_FAILURE;
    } else {
        for (std::map< std::string, vars_map >::const_iterator iter
             = md.test_cases.begin(); iter != md.test_cases.end(); iter++) {
            const std::string& tcname = (*iter).first;
            const vars_map& tcmd = (*iter).second;

            w.start_tc(tcname);

            try {
                const std::string& reqfail = tools::check_requirements(
                    tcmd, config);
                if (!reqfail.empty()) {
                    w.end_tc("skipped", reqfail);
                    continue;
                }
            } catch (const std::runtime_error& e) {
                w.end_tc("failed", e.what());
                errcode = EXIT_FAILURE;
                continue;
            }

            const std::pair< int, int > user = tools::get_required_user(
                tcmd, config);

            tools::fs::path resfile = resdir.get_path() / "tcr";
            assert(!tools::fs::exists(resfile));
            try {
                const bool has_cleanup = tools::text::to_bool(
                    (*tcmd.find("has.cleanup")).second);

                tools::fs::temp_dir workdir(tools::fs::path(tools::config::get(
                    "atf_workdir")) / "atf-run.XXXXXX");
                if (user.first != -1 && user.second != -1) {
                    if (::chown(workdir.get_path().c_str(), user.first,
                                user.second) == -1) {
                        throw tools::system_error("chown(" +
                            workdir.get_path().str() + ")", "chown(2) failed",
                            errno);
                    }
                    resfile = workdir.get_path() / "tcr";
                }

                std::pair< std::string, const tools::process::status > s =
                    tools::test_program::run_test_case(
                        tp, tcname, "body", tcmd, config,
                        resfile, workdir.get_path(), w);
                if (s.second.signaled() && s.second.coredump())
                    dump_stacktrace(tp, s.second, workdir.get_path(), w);
                if (has_cleanup)
                    (void)tools::test_program::run_test_case(
                        tp, tcname, "cleanup", tcmd,
                        config, resfile, workdir.get_path(), w);

                // TODO: Force deletion of workdir.

                tools::test_program::test_case_result tcr =
                    get_test_case_result(s.first, s.second, resfile);

                w.end_tc(tcr.state(), tcr.reason());
                if (tcr.state() == "failed")
                    errcode = EXIT_FAILURE;
            } catch (...) {
                if (tools::fs::exists(resfile))
                    tools::fs::remove(resfile);
                throw;
            }
            if (tools::fs::exists(resfile))
                tools::fs::remove(resfile);

        }
        w.end_tp("");
    }

    return errcode;
}

size_t
atf_run::count_tps(std::vector< std::string > tps)
    const
{
    size_t ntps = 0;

    for (std::vector< std::string >::const_iterator iter = tps.begin();
         iter != tps.end(); iter++) {
        tools::fs::path tp(*iter);
        tools::fs::file_info fi(tp);

        if (fi.get_type() == tools::fs::file_info::dir_type) {
            tools::atffile af = tools::read_atffile(tp / "Atffile");
            std::vector< std::string > aux = af.tps();
            for (std::vector< std::string >::iterator i2 = aux.begin();
                 i2 != aux.end(); i2++)
                *i2 = (tp / *i2).str();
            ntps += count_tps(aux);
        } else
            ntps++;
    }

    return ntps;
}

static
void
call_hook(const std::string& tool, const std::string& hook)
{
    const tools::fs::path sh(tools::config::get("atf_shell"));
    const tools::fs::path hooks =
        tools::fs::path(tools::config::get("atf_pkgdatadir")) / (tool + ".hooks");

    const tools::process::status s =
        tools::process::exec(sh,
                           tools::process::argv_array(sh.c_str(), hooks.c_str(),
                                                    hook.c_str(), NULL),
                           tools::process::stream_inherit(),
                           tools::process::stream_inherit());


    if (!s.exited() || s.exitstatus() != EXIT_SUCCESS)
        throw std::runtime_error("Failed to run the '" + hook + "' hook "
                                 "for '" + tool + "'");
}

int
atf_run::main(void)
{
    tools::atffile af = tools::read_atffile(tools::fs::path("Atffile"));

    std::vector< std::string > tps;
    tps = af.tps();
    if (m_argc >= 1) {
        // TODO: Ensure that the given test names are listed in the
        // Atffile.  Take into account that the file can be using globs.
        tps.clear();
        for (int i = 0; i < m_argc; i++)
            tps.push_back(m_argv[i]);
    }

    // Read configuration data for this test suite.
    vars_map test_suite_vars;
    {
        vars_map::const_iterator iter = af.props().find("test-suite");
        assert(iter != af.props().end());
        test_suite_vars = tools::config_file::read_config_files((*iter).second);
    }

    tools::test_program::atf_tps_writer w(std::cout);
    call_hook("atf-run", "info_start_hook");
    w.ntps(count_tps(tps));

    bool ok = true;
    for (std::vector< std::string >::const_iterator iter = tps.begin();
         iter != tps.end(); iter++) {
        const bool result = run_test(tools::fs::path(*iter), w,
            tools::config_file::merge_configs(af.conf(), test_suite_vars));
        ok &= (result == EXIT_SUCCESS);
    }

    call_hook("atf-run", "info_end_hook");

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
main(int argc, char* const* argv)
{
    return atf_run().run(argc, argv);
}

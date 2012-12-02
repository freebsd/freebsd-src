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

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
}

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

#include "atf-c/defs.h"

#include "atf-c++/detail/env.hpp"
#include "atf-c++/detail/parser.hpp"
#include "atf-c++/detail/process.hpp"
#include "atf-c++/detail/sanity.hpp"
#include "atf-c++/detail/text.hpp"

#include "config.hpp"
#include "fs.hpp"
#include "io.hpp"
#include "requirements.hpp"
#include "signals.hpp"
#include "test-program.hpp"
#include "timer.hpp"
#include "user.hpp"

namespace impl = atf::atf_run;
namespace detail = atf::atf_run::detail;

namespace {

static void
check_stream(std::ostream& os)
{
    // If we receive a signal while writing to the stream, the bad bit gets set.
    // Things seem to behave fine afterwards if we clear such error condition.
    // However, I'm not sure if it's safe to query errno at this point.
    if (os.bad()) {
        if (errno == EINTR)
            os.clear();
        else
            throw std::runtime_error("Failed");
    }
}

namespace atf_tp {

static const atf::parser::token_type eof_type = 0;
static const atf::parser::token_type nl_type = 1;
static const atf::parser::token_type text_type = 2;
static const atf::parser::token_type colon_type = 3;
static const atf::parser::token_type dblquote_type = 4;

class tokenizer : public atf::parser::tokenizer< std::istream > {
public:
    tokenizer(std::istream& is, size_t curline) :
        atf::parser::tokenizer< std::istream >
            (is, true, eof_type, nl_type, text_type, curline)
    {
        add_delim(':', colon_type);
        add_quote('"', dblquote_type);
    }
};

} // namespace atf_tp

class metadata_reader : public detail::atf_tp_reader {
    impl::test_cases_map m_tcs;

    void got_tc(const std::string& ident, const atf::tests::vars_map& props)
    {
        if (m_tcs.find(ident) != m_tcs.end())
            throw(std::runtime_error("Duplicate test case " + ident +
                                     " in test program"));
        m_tcs[ident] = props;

        if (m_tcs[ident].find("has.cleanup") == m_tcs[ident].end())
            m_tcs[ident].insert(std::make_pair("has.cleanup", "false"));

        if (m_tcs[ident].find("timeout") == m_tcs[ident].end())
            m_tcs[ident].insert(std::make_pair("timeout", "300"));
    }

public:
    metadata_reader(std::istream& is) :
        detail::atf_tp_reader(is)
    {
    }

    const impl::test_cases_map&
    get_tcs(void)
        const
    {
        return m_tcs;
    }
};

struct get_metadata_params {
    const atf::fs::path& executable;
    const atf::tests::vars_map& config;

    get_metadata_params(const atf::fs::path& p_executable,
                        const atf::tests::vars_map& p_config) :
        executable(p_executable),
        config(p_config)
    {
    }
};

struct test_case_params {
    const atf::fs::path& executable;
    const std::string& test_case_name;
    const std::string& test_case_part;
    const atf::tests::vars_map& metadata;
    const atf::tests::vars_map& config;
    const atf::fs::path& resfile;
    const atf::fs::path& workdir;

    test_case_params(const atf::fs::path& p_executable,
                     const std::string& p_test_case_name,
                     const std::string& p_test_case_part,
                     const atf::tests::vars_map& p_metadata,
                     const atf::tests::vars_map& p_config,
                     const atf::fs::path& p_resfile,
                     const atf::fs::path& p_workdir) :
        executable(p_executable),
        test_case_name(p_test_case_name),
        test_case_part(p_test_case_part),
        metadata(p_metadata),
        config(p_config),
        resfile(p_resfile),
        workdir(p_workdir)
    {
    }
};

static
std::string
generate_timestamp(void)
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == -1)
        return "0.0";

    char buf[32];
    const int len = snprintf(buf, sizeof(buf), "%ld.%ld",
                             static_cast< long >(tv.tv_sec),
                             static_cast< long >(tv.tv_usec));
    if (len >= static_cast< int >(sizeof(buf)) || len < 0)
        return "0.0";
    else
        return buf;
}

static
void
append_to_vector(std::vector< std::string >& v1,
                 const std::vector< std::string >& v2)
{
    std::copy(v2.begin(), v2.end(),
              std::back_insert_iterator< std::vector< std::string > >(v1));
}

static
char**
vector_to_argv(const std::vector< std::string >& v)
{
    char** argv = new char*[v.size() + 1];
    for (std::vector< std::string >::size_type i = 0; i < v.size(); i++) {
        argv[i] = strdup(v[i].c_str());
    }
    argv[v.size()] = NULL;
    return argv;
}

static
void
exec_or_exit(const atf::fs::path& executable,
             const std::vector< std::string >& argv)
{
    // This leaks memory in case of a failure, but it is OK.  Exiting will
    // do the necessary cleanup.
    char* const* native_argv = vector_to_argv(argv);

    ::execv(executable.c_str(), native_argv);

    const std::string message = "Failed to execute '" + executable.str() +
        "': " + std::strerror(errno) + "\n";
    if (::write(STDERR_FILENO, message.c_str(), message.length()) == -1)
        std::abort();
    std::exit(EXIT_FAILURE);
}

static
std::vector< std::string >
config_to_args(const atf::tests::vars_map& config)
{
    std::vector< std::string > args;

    for (atf::tests::vars_map::const_iterator iter = config.begin();
         iter != config.end(); iter++)
        args.push_back("-v" + (*iter).first + "=" + (*iter).second);

    return args;
}

static
void
silence_stdin(void)
{
    ::close(STDIN_FILENO);
    int fd = ::open("/dev/null", O_RDONLY);
    if (fd == -1)
        throw std::runtime_error("Could not open /dev/null");
    INV(fd == STDIN_FILENO);
}

static
void
prepare_child(const atf::fs::path& workdir)
{
    const int ret = ::setpgid(::getpid(), 0);
    INV(ret != -1);

    ::umask(S_IWGRP | S_IWOTH);

    for (int i = 1; i <= impl::last_signo; i++)
        impl::reset(i);

    atf::env::set("HOME", workdir.str());
    atf::env::unset("LANG");
    atf::env::unset("LC_ALL");
    atf::env::unset("LC_COLLATE");
    atf::env::unset("LC_CTYPE");
    atf::env::unset("LC_MESSAGES");
    atf::env::unset("LC_MONETARY");
    atf::env::unset("LC_NUMERIC");
    atf::env::unset("LC_TIME");
    atf::env::set("TZ", "UTC");

    atf::env::set("__RUNNING_INSIDE_ATF_RUN", "internal-yes-value");

    impl::change_directory(workdir);

    silence_stdin();
}

static
void
get_metadata_child(void* raw_params)
{
    const get_metadata_params* params =
        static_cast< const get_metadata_params* >(raw_params);

    std::vector< std::string > argv;
    argv.push_back(params->executable.leaf_name());
    argv.push_back("-l");
    argv.push_back("-s" + params->executable.branch_path().str());
    append_to_vector(argv, config_to_args(params->config));

    exec_or_exit(params->executable, argv);
}

void
run_test_case_child(void* raw_params)
{
    const test_case_params* params =
        static_cast< const test_case_params* >(raw_params);

    const std::pair< int, int > user = impl::get_required_user(
        params->metadata, params->config);
    if (user.first != -1 && user.second != -1)
        impl::drop_privileges(user);

    // The input 'tp' parameter may be relative and become invalid once
    // we change the current working directory.
    const atf::fs::path absolute_executable = params->executable.to_absolute();

    // Prepare the test program's arguments.  We use dynamic memory and
    // do not care to release it.  We are going to die anyway very soon,
    // either due to exec(2) or to exit(3).
    std::vector< std::string > argv;
    argv.push_back(absolute_executable.leaf_name());
    argv.push_back("-r" + params->resfile.str());
    argv.push_back("-s" + absolute_executable.branch_path().str());
    append_to_vector(argv, config_to_args(params->config));
    argv.push_back(params->test_case_name + ":" + params->test_case_part);

    prepare_child(params->workdir);
    exec_or_exit(absolute_executable, argv);
}

static void
tokenize_result(const std::string& line, std::string& out_state,
                std::string& out_arg, std::string& out_reason)
{
    const std::string::size_type pos = line.find_first_of(":(");
    if (pos == std::string::npos) {
        out_state = line;
        out_arg = "";
        out_reason = "";
    } else if (line[pos] == ':') {
        out_state = line.substr(0, pos);
        out_arg = "";
        out_reason = atf::text::trim(line.substr(pos + 1));
    } else if (line[pos] == '(') {
        const std::string::size_type pos2 = line.find("):", pos);
        if (pos2 == std::string::npos)
            throw std::runtime_error("Invalid test case result '" + line +
                "': unclosed optional argument");
        out_state = line.substr(0, pos);
        out_arg = line.substr(pos + 1, pos2 - pos - 1);
        out_reason = atf::text::trim(line.substr(pos2 + 2));
    } else
        UNREACHABLE;
}

static impl::test_case_result
handle_result(const std::string& state, const std::string& arg,
              const std::string& reason)
{
    PRE(state == "passed");

    if (!arg.empty() || !reason.empty())
        throw std::runtime_error("The test case result '" + state + "' cannot "
            "be accompanied by a reason nor an expected value");

    return impl::test_case_result(state, -1, reason);
}

static impl::test_case_result
handle_result_with_reason(const std::string& state, const std::string& arg,
                          const std::string& reason)
{
    PRE(state == "expected_death" || state == "expected_failure" ||
        state == "expected_timeout" || state == "failed" || state == "skipped");

    if (!arg.empty() || reason.empty())
        throw std::runtime_error("The test case result '" + state + "' must "
            "be accompanied by a reason but not by an expected value");

    return impl::test_case_result(state, -1, reason);
}

static impl::test_case_result
handle_result_with_reason_and_arg(const std::string& state,
                                  const std::string& arg,
                                  const std::string& reason)
{
    PRE(state == "expected_exit" || state == "expected_signal");

    if (reason.empty())
        throw std::runtime_error("The test case result '" + state + "' must "
            "be accompanied by a reason");

    int value;
    if (arg.empty()) {
        value = -1;
    } else {
        try {
            value = atf::text::to_type< int >(arg);
        } catch (const std::runtime_error&) {
            throw std::runtime_error("The value '" + arg + "' passed to the '" +
                state + "' state must be an integer");
        }
    }

    return impl::test_case_result(state, value, reason);
}

} // anonymous namespace

detail::atf_tp_reader::atf_tp_reader(std::istream& is) :
    m_is(is)
{
}

detail::atf_tp_reader::~atf_tp_reader(void)
{
}

void
detail::atf_tp_reader::got_tc(
    const std::string& ident ATF_DEFS_ATTRIBUTE_UNUSED,
    const std::map< std::string, std::string >& md ATF_DEFS_ATTRIBUTE_UNUSED)
{
}

void
detail::atf_tp_reader::got_eof(void)
{
}

void
detail::atf_tp_reader::validate_and_insert(const std::string& name,
    const std::string& value, const size_t lineno,
    std::map< std::string, std::string >& md)
{
    using atf::parser::parse_error;

    if (value.empty())
        throw parse_error(lineno, "The value for '" + name +"' cannot be "
                          "empty");

    const std::string ident_regex = "^[_A-Za-z0-9]+$";
    const std::string integer_regex = "^[0-9]+$";

    if (name == "descr") {
        // Any non-empty value is valid.
    } else if (name == "has.cleanup") {
        try {
            (void)atf::text::to_bool(value);
        } catch (const std::runtime_error&) {
            throw parse_error(lineno, "The has.cleanup property requires a"
                              " boolean value");
        }
    } else if (name == "ident") {
        if (!atf::text::match(value, ident_regex))
            throw parse_error(lineno, "The identifier must match " +
                              ident_regex + "; was '" + value + "'");
    } else if (name == "require.arch") {
    } else if (name == "require.config") {
    } else if (name == "require.files") {
    } else if (name == "require.machine") {
    } else if (name == "require.memory") {
        try {
            (void)atf::text::to_bytes(value);
        } catch (const std::runtime_error&) {
            throw parse_error(lineno, "The require.memory property requires an "
                              "integer value representing an amount of bytes");
        }
    } else if (name == "require.progs") {
    } else if (name == "require.user") {
    } else if (name == "timeout") {
        if (!atf::text::match(value, integer_regex))
            throw parse_error(lineno, "The timeout property requires an integer"
                              " value");
    } else if (name == "use.fs") {
        // Deprecated; ignore it.
    } else if (name.size() > 2 && name[0] == 'X' && name[1] == '-') {
        // Any non-empty value is valid.
    } else {
        throw parse_error(lineno, "Unknown property '" + name + "'");
    }

    md.insert(std::make_pair(name, value));
}

void
detail::atf_tp_reader::read(void)
{
    using atf::parser::parse_error;
    using namespace atf_tp;

    std::pair< size_t, atf::parser::headers_map > hml =
        atf::parser::read_headers(m_is, 1);
    atf::parser::validate_content_type(hml.second,
        "application/X-atf-tp", 1);

    tokenizer tkz(m_is, hml.first);
    atf::parser::parser< tokenizer > p(tkz);

    try {
        atf::parser::token t = p.expect(text_type, "property name");
        if (t.text() != "ident")
            throw parse_error(t.lineno(), "First property of a test case "
                              "must be 'ident'");

        std::map< std::string, std::string > props;
        do {
            const std::string name = t.text();
            t = p.expect(colon_type, "`:'");
            const std::string value = atf::text::trim(p.rest_of_line());
            t = p.expect(nl_type, "new line");
            validate_and_insert(name, value, t.lineno(), props);

            t = p.expect(eof_type, nl_type, text_type, "property name, new "
                         "line or eof");
            if (t.type() == nl_type || t.type() == eof_type) {
                const std::map< std::string, std::string >::const_iterator
                    iter = props.find("ident");
                if (iter == props.end())
                    throw parse_error(t.lineno(), "Test case definition did "
                                      "not define an 'ident' property");
                ATF_PARSER_CALLBACK(p, got_tc((*iter).second, props));
                props.clear();

                if (t.type() == nl_type) {
                    t = p.expect(text_type, "property name");
                    if (t.text() != "ident")
                        throw parse_error(t.lineno(), "First property of a "
                                          "test case must be 'ident'");
                }
            }
        } while (t.type() != eof_type);
        ATF_PARSER_CALLBACK(p, got_eof());
    } catch (const parse_error& pe) {
        p.add_error(pe);
        p.reset(nl_type);
    }
}

impl::test_case_result
detail::parse_test_case_result(const std::string& line)
{
    std::string state, arg, reason;
    tokenize_result(line, state, arg, reason);

    if (state == "expected_death")
        return handle_result_with_reason(state, arg, reason);
    else if (state.compare(0, 13, "expected_exit") == 0)
        return handle_result_with_reason_and_arg(state, arg, reason);
    else if (state.compare(0, 16, "expected_failure") == 0)
        return handle_result_with_reason(state, arg, reason);
    else if (state.compare(0, 15, "expected_signal") == 0)
        return handle_result_with_reason_and_arg(state, arg, reason);
    else if (state.compare(0, 16, "expected_timeout") == 0)
        return handle_result_with_reason(state, arg, reason);
    else if (state == "failed")
        return handle_result_with_reason(state, arg, reason);
    else if (state == "passed")
        return handle_result(state, arg, reason);
    else if (state == "skipped")
        return handle_result_with_reason(state, arg, reason);
    else
        throw std::runtime_error("Unknown test case result type in: " + line);
}

impl::atf_tps_writer::atf_tps_writer(std::ostream& os) :
    m_os(os)
{
    atf::parser::headers_map hm;
    atf::parser::attrs_map ct_attrs;
    ct_attrs["version"] = "3";
    hm["Content-Type"] =
        atf::parser::header_entry("Content-Type", "application/X-atf-tps",
                                  ct_attrs);
    atf::parser::write_headers(hm, m_os);
}

void
impl::atf_tps_writer::info(const std::string& what, const std::string& val)
{
    m_os << "info: " << what << ", " << val << "\n";
    m_os.flush();
}

void
impl::atf_tps_writer::ntps(size_t p_ntps)
{
    m_os << "tps-count: " << p_ntps << "\n";
    m_os.flush();
}

void
impl::atf_tps_writer::start_tp(const std::string& tp, size_t ntcs)
{
    m_tpname = tp;
    m_os << "tp-start: " << generate_timestamp() << ", " << tp << ", "
         << ntcs << "\n";
    m_os.flush();
}

void
impl::atf_tps_writer::end_tp(const std::string& reason)
{
    PRE(reason.find('\n') == std::string::npos);
    if (reason.empty())
        m_os << "tp-end: " << generate_timestamp() << ", " << m_tpname << "\n";
    else
        m_os << "tp-end: " << generate_timestamp() << ", " << m_tpname
             << ", " << reason << "\n";
    m_os.flush();
}

void
impl::atf_tps_writer::start_tc(const std::string& tcname)
{
    m_tcname = tcname;
    m_os << "tc-start: " << generate_timestamp() << ", " << tcname << "\n";
    m_os.flush();
}

void
impl::atf_tps_writer::stdout_tc(const std::string& line)
{
    m_os << "tc-so:" << line << "\n";
    check_stream(m_os);
    m_os.flush();
    check_stream(m_os);
}

void
impl::atf_tps_writer::stderr_tc(const std::string& line)
{
    m_os << "tc-se:" << line << "\n";
    check_stream(m_os);
    m_os.flush();
    check_stream(m_os);
}

void
impl::atf_tps_writer::end_tc(const std::string& state,
                             const std::string& reason)
{
    std::string str =  ", " + m_tcname + ", " + state;
    if (!reason.empty())
        str += ", " + reason;
    m_os << "tc-end: " << generate_timestamp() << str << "\n";
    m_os.flush();
}

impl::metadata
impl::get_metadata(const atf::fs::path& executable,
                   const atf::tests::vars_map& config)
{
    get_metadata_params params(executable, config);
    atf::process::child child =
        atf::process::fork(get_metadata_child,
                           atf::process::stream_capture(),
                           atf::process::stream_inherit(),
                           static_cast< void * >(&params));

    impl::pistream outin(child.stdout_fd());

    metadata_reader parser(outin);
    parser.read();

    const atf::process::status status = child.wait();
    if (!status.exited() || status.exitstatus() != EXIT_SUCCESS)
        throw atf::parser::format_error("Test program returned failure "
                                        "exit status for test case list");

    return metadata(parser.get_tcs());
}

impl::test_case_result
impl::read_test_case_result(const atf::fs::path& results_path)
{
    std::ifstream results_file(results_path.c_str());
    if (!results_file)
        throw std::runtime_error("Failed to open " + results_path.str());

    std::string line, extra_line;
    std::getline(results_file, line);
    if (!results_file.good())
        throw std::runtime_error("Results file is empty");

    while (std::getline(results_file, extra_line).good())
        line += "<<NEWLINE UNEXPECTED>>" + extra_line;

    results_file.close();

    return detail::parse_test_case_result(line);
}

namespace {

static volatile bool terminate_poll;

static void
sigchld_handler(const int signo ATF_DEFS_ATTRIBUTE_UNUSED)
{
    terminate_poll = true;
}

class child_muxer : public impl::muxer {
    impl::atf_tps_writer& m_writer;

    void
    line_callback(const size_t index, const std::string& line)
    {
        switch (index) {
        case 0: m_writer.stdout_tc(line); break;
        case 1: m_writer.stderr_tc(line); break;
        default: UNREACHABLE;
        }
    }

public:
    child_muxer(const int* fds, const size_t nfds,
                impl::atf_tps_writer& writer) :
        muxer(fds, nfds),
        m_writer(writer)
    {
    }
};

} // anonymous namespace

std::pair< std::string, atf::process::status >
impl::run_test_case(const atf::fs::path& executable,
                    const std::string& test_case_name,
                    const std::string& test_case_part,
                    const atf::tests::vars_map& metadata,
                    const atf::tests::vars_map& config,
                    const atf::fs::path& resfile,
                    const atf::fs::path& workdir,
                    atf_tps_writer& writer)
{
    // TODO: Capture termination signals and deliver them to the subprocess
    // instead.  Or maybe do something else; think about it.

    test_case_params params(executable, test_case_name, test_case_part,
                            metadata, config, resfile, workdir);
    atf::process::child child =
        atf::process::fork(run_test_case_child,
                           atf::process::stream_capture(),
                           atf::process::stream_capture(),
                           static_cast< void * >(&params));

    terminate_poll = false;

    const atf::tests::vars_map::const_iterator iter = metadata.find("timeout");
    INV(iter != metadata.end());
    const unsigned int timeout =
        atf::text::to_type< unsigned int >((*iter).second);
    const pid_t child_pid = child.pid();

    // Get the input stream of stdout and stderr.
    impl::file_handle outfh = child.stdout_fd();
    impl::file_handle errfh = child.stderr_fd();

    bool timed_out = false;

    // Process the test case's output and multiplex it into our output
    // stream as we read it.
    int fds[2] = {outfh.get(), errfh.get()};
    child_muxer mux(fds, 2, writer);
    try {
        child_timer timeout_timer(timeout, child_pid, terminate_poll);
        signal_programmer sigchld(SIGCHLD, sigchld_handler);
        mux.mux(terminate_poll);
        timed_out = timeout_timer.fired();
    } catch (...) {
        UNREACHABLE;
    }

    ::killpg(child_pid, SIGKILL);
    mux.flush();
    atf::process::status status = child.wait();

    std::string reason;

    if (timed_out) {
        // Don't assume the child process has been signaled due to the timeout
        // expiration as older versions did.  The child process may have exited
        // but we may have timed out due to a subchild process getting stuck.
        reason = "Test case timed out after " + atf::text::to_string(timeout) +
            " " + (timeout == 1 ? "second" : "seconds");
    }

    return std::make_pair(reason, status);
}

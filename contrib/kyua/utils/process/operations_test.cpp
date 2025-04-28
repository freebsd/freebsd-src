// Copyright 2014 The Kyua Authors.
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

#include "utils/process/operations.hpp"

extern "C" {
#include <sys/types.h>
#include <sys/wait.h>

#include <signal.h>
#include <unistd.h>
}

#include <cerrno>
#include <iostream>

#include <atf-c++.hpp>

#include "utils/defs.hpp"
#include "utils/format/containers.ipp"
#include "utils/fs/path.hpp"
#include "utils/process/child.ipp"
#include "utils/process/exceptions.hpp"
#include "utils/process/status.hpp"
#include "utils/stacktrace.hpp"
#include "utils/test_utils.ipp"

namespace fs = utils::fs;
namespace process = utils::process;


namespace {


/// Type of the process::exec() and process::exec_unsafe() functions.
typedef void (*exec_function)(const fs::path&, const process::args_vector&);


/// Calculates the path to the test helpers binary.
///
/// \param tc A pointer to the caller test case, needed to extract the value of
///     the "srcdir" property.
///
/// \return The path to the helpers binary.
static fs::path
get_helpers(const atf::tests::tc* tc)
{
    return fs::path(tc->get_config_var("srcdir")) / "helpers";
}


/// Body for a subprocess that runs exec().
class child_exec {
    /// Function to do the exec.
    const exec_function _do_exec;

    /// Path to the binary to exec.
    const fs::path& _program;

    /// Arguments to the binary, not including argv[0].
    const process::args_vector& _args;

public:
    /// Constructor.
    ///
    /// \param do_exec Function to do the exec.
    /// \param program Path to the binary to exec.
    /// \param args Arguments to the binary, not including argv[0].
    child_exec(const exec_function do_exec, const fs::path& program,
               const process::args_vector& args) :
        _do_exec(do_exec), _program(program), _args(args)
    {
    }

    /// Body for the subprocess.
    void
    operator()(void)
    {
        _do_exec(_program, _args);
    }
};


/// Body for a process that returns a specific exit code.
///
/// \tparam ExitStatus The exit status for the subprocess.
template< int ExitStatus >
static void
child_exit(void)
{
    std::exit(ExitStatus);
}


static void suspend(void) UTILS_NORETURN;


/// Blocks a subprocess from running indefinitely.
static void
suspend(void)
{
    sigset_t mask;
    sigemptyset(&mask);
    for (;;) {
        ::sigsuspend(&mask);
    }
}


static void write_loop(const int) UTILS_NORETURN;


/// Provides an infinite stream of data in a subprocess.
///
/// \param fd Descriptor into which to write.
static void
write_loop(const int fd)
{
    const int cookie = 0x12345678;
    for (;;) {
        std::cerr << "Still alive in PID " << ::getpid() << '\n';
        if (::write(fd, &cookie, sizeof(cookie)) != sizeof(cookie))
            std::exit(EXIT_FAILURE);
        ::sleep(1);
    }
}


}  // anonymous namespace


/// Tests an exec function with no arguments.
///
/// \param tc The calling test case.
/// \param do_exec The exec function to test.
static void
check_exec_no_args(const atf::tests::tc* tc, const exec_function do_exec)
{
    std::unique_ptr< process::child > child = process::child::fork_files(
        child_exec(do_exec, get_helpers(tc), process::args_vector()),
        fs::path("stdout"), fs::path("stderr"));
    const process::status status = child->wait();
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(EXIT_FAILURE, status.exitstatus());
    ATF_REQUIRE(atf::utils::grep_file("Must provide a helper name", "stderr"));
}


/// Tests an exec function with some arguments.
///
/// \param tc The calling test case.
/// \param do_exec The exec function to test.
static void
check_exec_some_args(const atf::tests::tc* tc, const exec_function do_exec)
{
    process::args_vector args;
    args.push_back("print-args");
    args.push_back("foo");
    args.push_back("bar");

    std::unique_ptr< process::child > child = process::child::fork_files(
        child_exec(do_exec, get_helpers(tc), args),
        fs::path("stdout"), fs::path("stderr"));
    const process::status status = child->wait();
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(EXIT_SUCCESS, status.exitstatus());
    ATF_REQUIRE(atf::utils::grep_file("argv\\[1\\] = print-args", "stdout"));
    ATF_REQUIRE(atf::utils::grep_file("argv\\[2\\] = foo", "stdout"));
    ATF_REQUIRE(atf::utils::grep_file("argv\\[3\\] = bar", "stdout"));
}


ATF_TEST_CASE_WITHOUT_HEAD(exec__no_args);
ATF_TEST_CASE_BODY(exec__no_args)
{
    check_exec_no_args(this, process::exec);
}


ATF_TEST_CASE_WITHOUT_HEAD(exec__some_args);
ATF_TEST_CASE_BODY(exec__some_args)
{
    check_exec_some_args(this, process::exec);
}


ATF_TEST_CASE_WITHOUT_HEAD(exec__fail);
ATF_TEST_CASE_BODY(exec__fail)
{
    utils::avoid_coredump_on_crash();

    std::unique_ptr< process::child > child = process::child::fork_files(
        child_exec(process::exec, fs::path("non-existent"),
                   process::args_vector()),
        fs::path("stdout"), fs::path("stderr"));
    const process::status status = child->wait();
    ATF_REQUIRE(status.signaled());
    ATF_REQUIRE_EQ(SIGABRT, status.termsig());
    ATF_REQUIRE(atf::utils::grep_file("Failed to execute non-existent",
                                      "stderr"));
}


ATF_TEST_CASE_WITHOUT_HEAD(exec_unsafe__no_args);
ATF_TEST_CASE_BODY(exec_unsafe__no_args)
{
    check_exec_no_args(this, process::exec_unsafe);
}


ATF_TEST_CASE_WITHOUT_HEAD(exec_unsafe__some_args);
ATF_TEST_CASE_BODY(exec_unsafe__some_args)
{
    check_exec_some_args(this, process::exec_unsafe);
}


ATF_TEST_CASE_WITHOUT_HEAD(exec_unsafe__fail);
ATF_TEST_CASE_BODY(exec_unsafe__fail)
{
    ATF_REQUIRE_THROW_RE(
        process::system_error, "Failed to execute missing-program",
        process::exec_unsafe(fs::path("missing-program"),
                             process::args_vector()));
}


ATF_TEST_CASE_WITHOUT_HEAD(terminate_group__setpgrp_executed);
ATF_TEST_CASE_BODY(terminate_group__setpgrp_executed)
{
    int first_fds[2], second_fds[2];
    ATF_REQUIRE(::pipe(first_fds) != -1);
    ATF_REQUIRE(::pipe(second_fds) != -1);

    const pid_t pid = ::fork();
    ATF_REQUIRE(pid != -1);
    if (pid == 0) {
        ::setpgid(::getpid(), ::getpid());
        const pid_t pid2 = ::fork();
        if (pid2 == -1) {
            std::exit(EXIT_FAILURE);
        } else if (pid2 == 0) {
            ::close(first_fds[0]);
            ::close(first_fds[1]);
            ::close(second_fds[0]);
            write_loop(second_fds[1]);
        }
        ::close(first_fds[0]);
        ::close(second_fds[0]);
        ::close(second_fds[1]);
        write_loop(first_fds[1]);
    }
    ::close(first_fds[1]);
    ::close(second_fds[1]);

    int dummy;
    std::cerr << "Waiting for children to start\n";
    while (::read(first_fds[0], &dummy, sizeof(dummy)) <= 0 ||
           ::read(second_fds[0], &dummy, sizeof(dummy)) <= 0) {
        // Wait for children to come up.
    }

    process::terminate_group(pid);
    std::cerr << "Waiting for children to die\n";
    while (::read(first_fds[0], &dummy, sizeof(dummy)) > 0 ||
           ::read(second_fds[0], &dummy, sizeof(dummy)) > 0) {
        // Wait for children to terminate.  If they don't, then the test case
        // will time out.
    }

    int status;
    ATF_REQUIRE(::wait(&status) != -1);
    ATF_REQUIRE(WIFSIGNALED(status));
    ATF_REQUIRE(WTERMSIG(status) == SIGKILL);
}


ATF_TEST_CASE_WITHOUT_HEAD(terminate_group__setpgrp_not_executed);
ATF_TEST_CASE_BODY(terminate_group__setpgrp_not_executed)
{
    const pid_t pid = ::fork();
    ATF_REQUIRE(pid != -1);
    if (pid == 0) {
        // We do not call setgprp() here to simulate the race that happens when
        // we invoke terminate_group on a process that has not yet had a chance
        // to run the setpgrp() call.
        suspend();
    }

    process::terminate_group(pid);

    int status;
    ATF_REQUIRE(::wait(&status) != -1);
    ATF_REQUIRE(WIFSIGNALED(status));
    ATF_REQUIRE(WTERMSIG(status) == SIGKILL);
}


ATF_TEST_CASE_WITHOUT_HEAD(terminate_self_with__exitstatus);
ATF_TEST_CASE_BODY(terminate_self_with__exitstatus)
{
    const pid_t pid = ::fork();
    ATF_REQUIRE(pid != -1);
    if (pid == 0) {
        const process::status status = process::status::fake_exited(123);
        process::terminate_self_with(status);
    }

    int status;
    ATF_REQUIRE(::wait(&status) != -1);
    ATF_REQUIRE(WIFEXITED(status));
    ATF_REQUIRE(WEXITSTATUS(status) == 123);
}


ATF_TEST_CASE_WITHOUT_HEAD(terminate_self_with__termsig);
ATF_TEST_CASE_BODY(terminate_self_with__termsig)
{
    const pid_t pid = ::fork();
    ATF_REQUIRE(pid != -1);
    if (pid == 0) {
        const process::status status = process::status::fake_signaled(
            SIGKILL, false);
        process::terminate_self_with(status);
    }

    int status;
    ATF_REQUIRE(::wait(&status) != -1);
    ATF_REQUIRE(WIFSIGNALED(status));
    ATF_REQUIRE(WTERMSIG(status) == SIGKILL);
    ATF_REQUIRE(!WCOREDUMP(status));
}


ATF_TEST_CASE_WITHOUT_HEAD(terminate_self_with__termsig_and_core);
ATF_TEST_CASE_BODY(terminate_self_with__termsig_and_core)
{
    utils::prepare_coredump_test(this);

    const pid_t pid = ::fork();
    ATF_REQUIRE(pid != -1);
    if (pid == 0) {
        const process::status status = process::status::fake_signaled(
            SIGABRT, true);
        process::terminate_self_with(status);
    }

    int status;
    ATF_REQUIRE(::wait(&status) != -1);
    ATF_REQUIRE(WIFSIGNALED(status));
    ATF_REQUIRE(WTERMSIG(status) == SIGABRT);
    ATF_REQUIRE(WCOREDUMP(status));
}


ATF_TEST_CASE_WITHOUT_HEAD(wait__ok);
ATF_TEST_CASE_BODY(wait__ok)
{
    std::unique_ptr< process::child > child = process::child::fork_capture(
        child_exit< 15 >);
    const pid_t pid = child->pid();
    child.reset();  // Ensure there is no conflict between destructor and wait.

    const process::status status = process::wait(pid);
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(15, status.exitstatus());
}


ATF_TEST_CASE_WITHOUT_HEAD(wait__fail);
ATF_TEST_CASE_BODY(wait__fail)
{
    ATF_REQUIRE_THROW(process::system_error, process::wait(1));
}


ATF_TEST_CASE_WITHOUT_HEAD(wait_any__one);
ATF_TEST_CASE_BODY(wait_any__one)
{
    process::child::fork_capture(child_exit< 15 >);

    const process::status status = process::wait_any();
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(15, status.exitstatus());
}


ATF_TEST_CASE_WITHOUT_HEAD(wait_any__many);
ATF_TEST_CASE_BODY(wait_any__many)
{
    process::child::fork_capture(child_exit< 15 >);
    process::child::fork_capture(child_exit< 30 >);
    process::child::fork_capture(child_exit< 45 >);

    std::set< int > exit_codes;
    for (int i = 0; i < 3; i++) {
        const process::status status = process::wait_any();
        ATF_REQUIRE(status.exited());
        exit_codes.insert(status.exitstatus());
    }

    std::set< int > exp_exit_codes;
    exp_exit_codes.insert(15);
    exp_exit_codes.insert(30);
    exp_exit_codes.insert(45);
    ATF_REQUIRE_EQ(exp_exit_codes, exit_codes);
}


ATF_TEST_CASE_WITHOUT_HEAD(wait_any__none_is_failure);
ATF_TEST_CASE_BODY(wait_any__none_is_failure)
{
    try {
        const process::status status = process::wait_any();
        fail("Expected exception but none raised");
    } catch (const process::system_error& e) {
        ATF_REQUIRE(atf::utils::grep_string("Failed to wait", e.what()));
        ATF_REQUIRE_EQ(ECHILD, e.original_errno());
    }
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, exec__no_args);
    ATF_ADD_TEST_CASE(tcs, exec__some_args);
    ATF_ADD_TEST_CASE(tcs, exec__fail);

    ATF_ADD_TEST_CASE(tcs, exec_unsafe__no_args);
    ATF_ADD_TEST_CASE(tcs, exec_unsafe__some_args);
    ATF_ADD_TEST_CASE(tcs, exec_unsafe__fail);

    ATF_ADD_TEST_CASE(tcs, terminate_group__setpgrp_executed);
    ATF_ADD_TEST_CASE(tcs, terminate_group__setpgrp_not_executed);

    ATF_ADD_TEST_CASE(tcs, terminate_self_with__exitstatus);
    ATF_ADD_TEST_CASE(tcs, terminate_self_with__termsig);
    ATF_ADD_TEST_CASE(tcs, terminate_self_with__termsig_and_core);

    ATF_ADD_TEST_CASE(tcs, wait__ok);
    ATF_ADD_TEST_CASE(tcs, wait__fail);

    ATF_ADD_TEST_CASE(tcs, wait_any__one);
    ATF_ADD_TEST_CASE(tcs, wait_any__many);
    ATF_ADD_TEST_CASE(tcs, wait_any__none_is_failure);
}

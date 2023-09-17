// Copyright 2015 The Kyua Authors.
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

#include "utils/process/executor.ipp"

extern "C" {
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <signal.h>
#include <unistd.h>
}

#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

#include <atf-c++.hpp>

#include "utils/datetime.hpp"
#include "utils/defs.hpp"
#include "utils/env.hpp"
#include "utils/format/containers.ipp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/passwd.hpp"
#include "utils/process/status.hpp"
#include "utils/sanity.hpp"
#include "utils/signals/exceptions.hpp"
#include "utils/stacktrace.hpp"
#include "utils/text/exceptions.hpp"
#include "utils/text/operations.ipp"

namespace datetime = utils::datetime;
namespace executor = utils::process::executor;
namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace process = utils::process;
namespace signals = utils::signals;
namespace text = utils::text;

using utils::none;
using utils::optional;


/// Large timeout for the processes we spawn.
///
/// This number is supposed to be (much) larger than the timeout of the test
/// cases that use it so that children processes are not killed spuriously.
static const datetime::delta infinite_timeout(1000000, 0);


static void do_exit(const int) UTILS_NORETURN;


/// Terminates a subprocess without invoking destructors.
///
/// This is just a simple wrapper over _exit(2) because we cannot use std::exit
/// on exit from a subprocess.  The reason is that we do not want to invoke any
/// destructors as otherwise we'd clear up the global executor state by mistake.
/// This wouldn't be a major problem if it wasn't because doing so deletes
/// on-disk files and we want to leave them in place so that the parent process
/// can test for them!
///
/// \param exit_code Code to exit with.
static void
do_exit(const int exit_code)
{
    std::cout.flush();
    std::cerr.flush();
    ::_exit(exit_code);
}


/// Subprocess that creates a cookie file in its work directory.
class child_create_cookie {
    /// Name of the cookie to create.
    const std::string _cookie_name;

public:
    /// Constructor.
    ///
    /// \param cookie_name Name of the cookie to create.
    child_create_cookie(const std::string& cookie_name) :
        _cookie_name(cookie_name)
    {
    }

    /// Runs the subprocess.
    void
    operator()(const fs::path& /* control_directory */)
        UTILS_NORETURN
    {
        std::cout << "Creating cookie: " << _cookie_name << " (stdout)\n";
        std::cerr << "Creating cookie: " << _cookie_name << " (stderr)\n";
        atf::utils::create_file(_cookie_name, "");
        do_exit(EXIT_SUCCESS);
    }
};


static void child_delete_all(const fs::path&) UTILS_NORETURN;


/// Subprocess that deletes all files in the current directory.
///
/// This is intended to validate that the test runs in an empty directory,
/// separate from any control files that the executor may have created.
///
/// \param control_directory Directory where control files separate from the
///     work directory can be placed.
static void
child_delete_all(const fs::path& control_directory)
{
    const fs::path cookie = control_directory / "exec_was_called";
    std::ofstream control_file(cookie.c_str());
    if (!control_file) {
        std::cerr << "Failed to create " << cookie << '\n';
        std::abort();
    }

    const int exit_code = ::system("rm *") == -1
        ? EXIT_FAILURE : EXIT_SUCCESS;
    do_exit(exit_code);
}


static void child_dump_unprivileged_user(const fs::path&) UTILS_NORETURN;


/// Subprocess that dumps user configuration.
static void
child_dump_unprivileged_user(const fs::path& /* control_directory */)
{
    const passwd::user current_user = passwd::current_user();
    std::cout << F("UID = %s\n") % current_user.uid;
    do_exit(EXIT_SUCCESS);
}


/// Subprocess that returns a specific exit code.
class child_exit {
    /// Exit code to return.
    int _exit_code;

public:
    /// Constructor.
    ///
    /// \param exit_code Exit code to return.
    child_exit(const int exit_code) : _exit_code(exit_code)
    {
    }

    /// Runs the subprocess.
    void
    operator()(const fs::path& /* control_directory */)
        UTILS_NORETURN
    {
        do_exit(_exit_code);
    }
};


static void child_pause(const fs::path&) UTILS_NORETURN;


/// Subprocess that just blocks.
static void
child_pause(const fs::path& /* control_directory */)
{
    sigset_t mask;
    sigemptyset(&mask);
    for (;;) {
        ::sigsuspend(&mask);
    }
    std::abort();
}


static void child_print(const fs::path&) UTILS_NORETURN;


/// Subprocess that writes to stdout and stderr.
static void
child_print(const fs::path& /* control_directory */)
{
    std::cout << "stdout: some text\n";
    std::cerr << "stderr: some other text\n";

    do_exit(EXIT_SUCCESS);
}


/// Subprocess that sleeps for a period of time before exiting.
class child_sleep {
    /// Seconds to sleep for before termination.
    int _seconds;

public:
    /// Construtor.
    ///
    /// \param seconds Seconds to sleep for before termination.
    child_sleep(const int seconds) : _seconds(seconds)
    {
    }

    /// Runs the subprocess.
    void
    operator()(const fs::path& /* control_directory */)
        UTILS_NORETURN
    {
        ::sleep(_seconds);
        do_exit(EXIT_SUCCESS);
    }
};


static void child_spawn_blocking_child(const fs::path&) UTILS_NORETURN;


/// Subprocess that spawns a subchild that gets stuck.
///
/// Used by the caller to validate that the whole process tree is terminated
/// when this subprocess is killed.
static void
child_spawn_blocking_child(
    const fs::path& /* control_directory */)
{
    pid_t pid = ::fork();
    if (pid == -1) {
        std::cerr << "Cannot fork subprocess\n";
        do_exit(EXIT_FAILURE);
    } else if (pid == 0) {
        for (;;)
            ::pause();
    } else {
        const fs::path name = fs::path(utils::getenv("CONTROL_DIR").get()) /
            "pid";
        std::ofstream pidfile(name.c_str());
        if (!pidfile) {
            std::cerr << "Failed to create the pidfile\n";
            do_exit(EXIT_FAILURE);
        }
        pidfile << pid;
        pidfile.close();
        do_exit(EXIT_SUCCESS);
    }
}


static void child_validate_isolation(const fs::path&) UTILS_NORETURN;


/// Subprocess that checks if isolate_child() has been called.
static void
child_validate_isolation(const fs::path& /* control_directory */)
{
    if (utils::getenv("HOME").get() == "fake-value") {
        std::cerr << "HOME not reset\n";
        do_exit(EXIT_FAILURE);
    }
    if (utils::getenv("LANG")) {
        std::cerr << "LANG not unset\n";
        do_exit(EXIT_FAILURE);
    }
    do_exit(EXIT_SUCCESS);
}


/// Invokes executor::spawn() with default arguments.
///
/// \param handle The executor on which to invoke spawn().
/// \param args Arguments to the binary.
/// \param timeout Maximum time the program can run for.
/// \param unprivileged_user If set, user to switch to when running the child
///     program.
/// \param stdout_target If not none, file to which to write the stdout of the
///     test case.
/// \param stderr_target If not none, file to which to write the stderr of the
///     test case.
///
/// \return The exec handle for the spawned binary.
template< class Hook >
static executor::exec_handle
do_spawn(executor::executor_handle& handle, Hook hook,
         const datetime::delta& timeout = infinite_timeout,
         const optional< passwd::user > unprivileged_user = none,
         const optional< fs::path > stdout_target = none,
         const optional< fs::path > stderr_target = none)
{
    const executor::exec_handle exec_handle = handle.spawn< Hook >(
        hook, timeout, unprivileged_user, stdout_target, stderr_target);
    return exec_handle;
}


/// Checks for a specific exit status in the status of a exit_handle.
///
/// \param exit_status The expected exit status.
/// \param status The value of exit_handle.status().
///
/// \post Terminates the calling test case if the status does not match the
/// required value.
static void
require_exit(const int exit_status, const optional< process::status > status)
{
    ATF_REQUIRE(status);
    ATF_REQUIRE(status.get().exited());
    ATF_REQUIRE_EQ(exit_status, status.get().exitstatus());
}


/// Ensures that a killed process is gone.
///
/// The way we do this is by sending an idempotent signal to the given PID
/// and checking if the signal was delivered.  If it was, the process is
/// still alive; if it was not, then it is gone.
///
/// Note that this might be inaccurate for two reasons:
///
/// 1) The system may have spawned a new process with the same pid as
///    our subchild... but in practice, this does not happen because
///    most systems do not immediately reuse pid numbers.  If that
///    happens... well, we get a false test failure.
///
/// 2) We ran so fast that even if the process was sent a signal to
///    die, it has not had enough time to process it yet.  This is why
///    we retry this a few times.
///
/// \param pid PID of the process to check.
static void
ensure_dead(const pid_t pid)
{
    int attempts = 30;
retry:
    if (::kill(pid, SIGCONT) != -1 || errno != ESRCH) {
        if (attempts > 0) {
            std::cout << "Subprocess not dead yet; retrying wait\n";
            --attempts;
            ::usleep(100000);
            goto retry;
        }
        ATF_FAIL(F("The subprocess %s of our child was not killed") % pid);
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__run_one);
ATF_TEST_CASE_BODY(integration__run_one)
{
    executor::executor_handle handle = executor::setup();

    const executor::exec_handle exec_handle = do_spawn(handle, child_exit(41));

    executor::exit_handle exit_handle = handle.wait_any();
    ATF_REQUIRE_EQ(exec_handle.pid(), exit_handle.original_pid());
    require_exit(41, exit_handle.status());
    exit_handle.cleanup();

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__run_many);
ATF_TEST_CASE_BODY(integration__run_many)
{
    static const std::size_t num_children = 30;

    executor::executor_handle handle = executor::setup();

    std::size_t total_children = 0;
    std::map< int, int > exp_exit_statuses;
    std::map< int, datetime::timestamp > exp_start_times;
    for (std::size_t i = 0; i < num_children; ++i) {
        const datetime::timestamp start_time = datetime::timestamp::from_values(
            2014, 12, 8, 9, 40, 0, i);

        for (std::size_t j = 0; j < 3; j++) {
            const std::size_t id = i * 3 + j;

            datetime::set_mock_now(start_time);
            const int pid = do_spawn(handle, child_exit(id)).pid();
            exp_exit_statuses.insert(std::make_pair(pid, id));
            exp_start_times.insert(std::make_pair(pid, start_time));
            ++total_children;
        }
    }

    for (std::size_t i = 0; i < total_children; ++i) {
        const datetime::timestamp end_time = datetime::timestamp::from_values(
            2014, 12, 8, 9, 50, 10, i);
        datetime::set_mock_now(end_time);
        executor::exit_handle exit_handle = handle.wait_any();
        const int original_pid = exit_handle.original_pid();

        const int exit_status = exp_exit_statuses.find(original_pid)->second;
        const datetime::timestamp& start_time = exp_start_times.find(
            original_pid)->second;

        require_exit(exit_status, exit_handle.status());

        ATF_REQUIRE_EQ(start_time, exit_handle.start_time());
        ATF_REQUIRE_EQ(end_time, exit_handle.end_time());

        exit_handle.cleanup();

        ATF_REQUIRE(!atf::utils::file_exists(
                        exit_handle.stdout_file().str()));
        ATF_REQUIRE(!atf::utils::file_exists(
                        exit_handle.stderr_file().str()));
        ATF_REQUIRE(!atf::utils::file_exists(
                        exit_handle.work_directory().str()));
    }

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__parameters_and_output);
ATF_TEST_CASE_BODY(integration__parameters_and_output)
{
    executor::executor_handle handle = executor::setup();

    const executor::exec_handle exec_handle = do_spawn(handle, child_print);

    executor::exit_handle exit_handle = handle.wait_any();

    ATF_REQUIRE_EQ(exec_handle.pid(), exit_handle.original_pid());

    require_exit(EXIT_SUCCESS, exit_handle.status());

    const fs::path stdout_file = exit_handle.stdout_file();
    ATF_REQUIRE(atf::utils::compare_file(
        stdout_file.str(), "stdout: some text\n"));
    const fs::path stderr_file = exit_handle.stderr_file();
    ATF_REQUIRE(atf::utils::compare_file(
        stderr_file.str(), "stderr: some other text\n"));

    exit_handle.cleanup();
    ATF_REQUIRE(!fs::exists(stdout_file));
    ATF_REQUIRE(!fs::exists(stderr_file));

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__custom_output_files);
ATF_TEST_CASE_BODY(integration__custom_output_files)
{
    executor::executor_handle handle = executor::setup();

    const fs::path stdout_file("custom-stdout.txt");
    const fs::path stderr_file("custom-stderr.txt");

    const executor::exec_handle exec_handle = do_spawn(
        handle, child_print, infinite_timeout, none,
        utils::make_optional(stdout_file),
        utils::make_optional(stderr_file));

    executor::exit_handle exit_handle = handle.wait_any();

    ATF_REQUIRE_EQ(exec_handle.pid(), exit_handle.original_pid());

    require_exit(EXIT_SUCCESS, exit_handle.status());

    ATF_REQUIRE_EQ(stdout_file, exit_handle.stdout_file());
    ATF_REQUIRE_EQ(stderr_file, exit_handle.stderr_file());

    exit_handle.cleanup();

    handle.cleanup();

    // Must compare after cleanup to ensure the files did not get deleted.
    ATF_REQUIRE(atf::utils::compare_file(
        stdout_file.str(), "stdout: some text\n"));
    ATF_REQUIRE(atf::utils::compare_file(
        stderr_file.str(), "stderr: some other text\n"));
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__timestamps);
ATF_TEST_CASE_BODY(integration__timestamps)
{
    executor::executor_handle handle = executor::setup();

    const datetime::timestamp start_time = datetime::timestamp::from_values(
        2014, 12, 8, 9, 35, 10, 1000);
    const datetime::timestamp end_time = datetime::timestamp::from_values(
        2014, 12, 8, 9, 35, 20, 2000);

    datetime::set_mock_now(start_time);
    do_spawn(handle, child_exit(70));

    datetime::set_mock_now(end_time);
    executor::exit_handle exit_handle = handle.wait_any();

    require_exit(70, exit_handle.status());

    ATF_REQUIRE_EQ(start_time, exit_handle.start_time());
    ATF_REQUIRE_EQ(end_time, exit_handle.end_time());
    exit_handle.cleanup();

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__files);
ATF_TEST_CASE_BODY(integration__files)
{
    executor::executor_handle handle = executor::setup();

    do_spawn(handle, child_create_cookie("cookie.12345"));

    executor::exit_handle exit_handle = handle.wait_any();

    ATF_REQUIRE(atf::utils::file_exists(
                    (exit_handle.work_directory() / "cookie.12345").str()));

    exit_handle.cleanup();

    ATF_REQUIRE(!atf::utils::file_exists(exit_handle.stdout_file().str()));
    ATF_REQUIRE(!atf::utils::file_exists(exit_handle.stderr_file().str()));
    ATF_REQUIRE(!atf::utils::file_exists(exit_handle.work_directory().str()));

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__followup);
ATF_TEST_CASE_BODY(integration__followup)
{
    executor::executor_handle handle = executor::setup();

    (void)handle.spawn(child_create_cookie("cookie.1"), infinite_timeout, none);
    executor::exit_handle exit_1_handle = handle.wait_any();

    (void)handle.spawn_followup(child_create_cookie("cookie.2"), exit_1_handle,
                                infinite_timeout);
    executor::exit_handle exit_2_handle = handle.wait_any();

    ATF_REQUIRE_EQ(exit_1_handle.stdout_file(), exit_2_handle.stdout_file());
    ATF_REQUIRE_EQ(exit_1_handle.stderr_file(), exit_2_handle.stderr_file());
    ATF_REQUIRE_EQ(exit_1_handle.control_directory(),
                   exit_2_handle.control_directory());
    ATF_REQUIRE_EQ(exit_1_handle.work_directory(),
                   exit_2_handle.work_directory());

    (void)handle.spawn_followup(child_create_cookie("cookie.3"), exit_2_handle,
                                infinite_timeout);
    exit_2_handle.cleanup();
    exit_1_handle.cleanup();
    executor::exit_handle exit_3_handle = handle.wait_any();

    ATF_REQUIRE_EQ(exit_1_handle.stdout_file(), exit_3_handle.stdout_file());
    ATF_REQUIRE_EQ(exit_1_handle.stderr_file(), exit_3_handle.stderr_file());
    ATF_REQUIRE_EQ(exit_1_handle.control_directory(),
                   exit_3_handle.control_directory());
    ATF_REQUIRE_EQ(exit_1_handle.work_directory(),
                   exit_3_handle.work_directory());

    ATF_REQUIRE(atf::utils::file_exists(
                    (exit_1_handle.work_directory() / "cookie.1").str()));
    ATF_REQUIRE(atf::utils::file_exists(
                    (exit_1_handle.work_directory() / "cookie.2").str()));
    ATF_REQUIRE(atf::utils::file_exists(
                    (exit_1_handle.work_directory() / "cookie.3").str()));

    ATF_REQUIRE(atf::utils::compare_file(
                    exit_1_handle.stdout_file().str(),
                    "Creating cookie: cookie.1 (stdout)\n"
                    "Creating cookie: cookie.2 (stdout)\n"
                    "Creating cookie: cookie.3 (stdout)\n"));

    ATF_REQUIRE(atf::utils::compare_file(
                    exit_1_handle.stderr_file().str(),
                    "Creating cookie: cookie.1 (stderr)\n"
                    "Creating cookie: cookie.2 (stderr)\n"
                    "Creating cookie: cookie.3 (stderr)\n"));

    exit_3_handle.cleanup();

    ATF_REQUIRE(!atf::utils::file_exists(exit_1_handle.stdout_file().str()));
    ATF_REQUIRE(!atf::utils::file_exists(exit_1_handle.stderr_file().str()));
    ATF_REQUIRE(!atf::utils::file_exists(exit_1_handle.work_directory().str()));

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__output_files_always_exist);
ATF_TEST_CASE_BODY(integration__output_files_always_exist)
{
    executor::executor_handle handle = executor::setup();

    // This test is racy: we specify a very short timeout for the subprocess so
    // that we cause the subprocess to exit before it has had time to set up the
    // output files.  However, for scheduling reasons, the subprocess may
    // actually run to completion before the timer triggers.  Retry this a few
    // times to attempt to catch a "good test".
    for (int i = 0; i < 50; i++) {
        const executor::exec_handle exec_handle =
            do_spawn(handle, child_exit(0), datetime::delta(0, 100000));
        executor::exit_handle exit_handle = handle.wait(exec_handle);
        ATF_REQUIRE(fs::exists(exit_handle.stdout_file()));
        ATF_REQUIRE(fs::exists(exit_handle.stderr_file()));
        exit_handle.cleanup();
    }

    handle.cleanup();
}


ATF_TEST_CASE(integration__timeouts);
ATF_TEST_CASE_HEAD(integration__timeouts)
{
    set_md_var("timeout", "60");
}
ATF_TEST_CASE_BODY(integration__timeouts)
{
    executor::executor_handle handle = executor::setup();

    const executor::exec_handle exec_handle1 =
        do_spawn(handle, child_sleep(30), datetime::delta(2, 0));
    const executor::exec_handle exec_handle2 =
        do_spawn(handle, child_sleep(40), datetime::delta(5, 0));
    const executor::exec_handle exec_handle3 =
        do_spawn(handle, child_exit(15));

    {
        executor::exit_handle exit_handle = handle.wait_any();
        ATF_REQUIRE_EQ(exec_handle3.pid(), exit_handle.original_pid());
        require_exit(15, exit_handle.status());
        exit_handle.cleanup();
    }

    {
        executor::exit_handle exit_handle = handle.wait_any();
        ATF_REQUIRE_EQ(exec_handle1.pid(), exit_handle.original_pid());
        ATF_REQUIRE(!exit_handle.status());
        const datetime::delta duration =
            exit_handle.end_time() - exit_handle.start_time();
        ATF_REQUIRE(duration < datetime::delta(10, 0));
        ATF_REQUIRE(duration >= datetime::delta(2, 0));
        exit_handle.cleanup();
    }

    {
        executor::exit_handle exit_handle = handle.wait_any();
        ATF_REQUIRE_EQ(exec_handle2.pid(), exit_handle.original_pid());
        ATF_REQUIRE(!exit_handle.status());
        const datetime::delta duration =
            exit_handle.end_time() - exit_handle.start_time();
        ATF_REQUIRE(duration < datetime::delta(10, 0));
        ATF_REQUIRE(duration >= datetime::delta(4, 0));
        exit_handle.cleanup();
    }

    handle.cleanup();
}


ATF_TEST_CASE(integration__unprivileged_user);
ATF_TEST_CASE_HEAD(integration__unprivileged_user)
{
    set_md_var("require.config", "unprivileged-user");
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(integration__unprivileged_user)
{
    executor::executor_handle handle = executor::setup();

    const passwd::user unprivileged_user = passwd::find_user_by_name(
        get_config_var("unprivileged-user"));

    do_spawn(handle, child_dump_unprivileged_user,
             infinite_timeout, utils::make_optional(unprivileged_user));

    executor::exit_handle exit_handle = handle.wait_any();
    ATF_REQUIRE(atf::utils::compare_file(
        exit_handle.stdout_file().str(),
        F("UID = %s\n") % unprivileged_user.uid));
    exit_handle.cleanup();

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__auto_cleanup);
ATF_TEST_CASE_BODY(integration__auto_cleanup)
{
    std::vector< int > pids;
    std::vector< fs::path > paths;
    {
        executor::executor_handle handle = executor::setup();

        pids.push_back(do_spawn(handle, child_exit(10)).pid());
        pids.push_back(do_spawn(handle, child_exit(20)).pid());

        // This invocation is never waited for below.  This is intentional: we
        // want the destructor to clean the "leaked" test automatically so that
        // the clean up of the parent work directory also happens correctly.
        pids.push_back(do_spawn(handle, child_pause).pid());

        executor::exit_handle exit_handle1 = handle.wait_any();
        paths.push_back(exit_handle1.stdout_file());
        paths.push_back(exit_handle1.stderr_file());
        paths.push_back(exit_handle1.work_directory());

        executor::exit_handle exit_handle2 = handle.wait_any();
        paths.push_back(exit_handle2.stdout_file());
        paths.push_back(exit_handle2.stderr_file());
        paths.push_back(exit_handle2.work_directory());
    }
    for (std::vector< int >::const_iterator iter = pids.begin();
         iter != pids.end(); ++iter) {
        ensure_dead(*iter);
    }
    for (std::vector< fs::path >::const_iterator iter = paths.begin();
         iter != paths.end(); ++iter) {
        ATF_REQUIRE(!atf::utils::file_exists((*iter).str()));
    }
}


/// Ensures that interrupting an executor cleans things up correctly.
///
/// This test scenario is tricky.  We spawn a master child process that runs the
/// executor code and we send a signal to it externally.  The child process
/// spawns a bunch of tests that block indefinitely and tries to wait for their
/// results.  When the signal is received, we expect an interrupt_error to be
/// raised, which in turn should clean up all test resources and exit the master
/// child process successfully.
///
/// \param signo Signal to deliver to the executor.
static void
do_signal_handling_test(const int signo)
{
    static const char* cookie = "spawned.txt";

    const pid_t pid = ::fork();
    ATF_REQUIRE(pid != -1);
    if (pid == 0) {
        static const std::size_t num_children = 3;

        optional< fs::path > root_work_directory;
        try {
            executor::executor_handle handle = executor::setup();
            root_work_directory = handle.root_work_directory();

            for (std::size_t i = 0; i < num_children; ++i) {
                std::cout << "Spawned child number " << i << '\n';
                do_spawn(handle, child_pause);
            }

            std::cout << "Creating " << cookie << " cookie\n";
            atf::utils::create_file(cookie, "");

            std::cout << "Waiting for subprocess termination\n";
            for (std::size_t i = 0; i < num_children; ++i) {
                executor::exit_handle exit_handle = handle.wait_any();
                // We may never reach this point in the test, but if we do let's
                // make sure the subprocess was terminated as expected.
                if (exit_handle.status()) {
                    if (exit_handle.status().get().signaled() &&
                        exit_handle.status().get().termsig() == SIGKILL) {
                        // OK.
                    } else {
                        std::cerr << "Child exited with unexpected code: "
                                  << exit_handle.status().get();
                        std::exit(EXIT_FAILURE);
                    }
                } else {
                    std::cerr << "Child timed out\n";
                    std::exit(EXIT_FAILURE);
                }
                exit_handle.cleanup();
            }
            std::cerr << "Terminating without reception of signal\n";
            std::exit(EXIT_FAILURE);
        } catch (const signals::interrupted_error& unused_error) {
            std::cerr << "Terminating due to interrupted_error\n";
            // We never kill ourselves until the cookie is created, so it is
            // guaranteed that the optional root_work_directory has been
            // initialized at this point.
            if (atf::utils::file_exists(root_work_directory.get().str())) {
                // Some cleanup did not happen; error out.
                std::exit(EXIT_FAILURE);
            } else {
                std::exit(EXIT_SUCCESS);
            }
        }
        std::abort();
    }

    std::cout << "Waiting for " << cookie << " cookie creation\n";
    while (!atf::utils::file_exists(cookie)) {
        // Wait for processes.
    }
    ATF_REQUIRE(::unlink(cookie) != -1);
    std::cout << "Killing process\n";
    ATF_REQUIRE(::kill(pid, signo) != -1);

    int status;
    std::cout << "Waiting for process termination\n";
    ATF_REQUIRE(::waitpid(pid, &status, 0) != -1);
    ATF_REQUIRE(WIFEXITED(status));
    ATF_REQUIRE_EQ(EXIT_SUCCESS, WEXITSTATUS(status));
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__signal_handling);
ATF_TEST_CASE_BODY(integration__signal_handling)
{
    // This test scenario is racy so run it multiple times to have higher
    // chances of exposing problems.
    const std::size_t rounds = 20;

    for (std::size_t i = 0; i < rounds; ++i) {
        std::cout << F("Testing round %s\n") % i;
        do_signal_handling_test(SIGHUP);
        do_signal_handling_test(SIGINT);
        do_signal_handling_test(SIGTERM);
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__isolate_child_is_called);
ATF_TEST_CASE_BODY(integration__isolate_child_is_called)
{
    executor::executor_handle handle = executor::setup();

    utils::setenv("HOME", "fake-value");
    utils::setenv("LANG", "es_ES");
    do_spawn(handle, child_validate_isolation);

    executor::exit_handle exit_handle = handle.wait_any();
    require_exit(EXIT_SUCCESS, exit_handle.status());
    exit_handle.cleanup();

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__process_group_is_terminated);
ATF_TEST_CASE_BODY(integration__process_group_is_terminated)
{
    utils::setenv("CONTROL_DIR", fs::current_path().str());

    executor::executor_handle handle = executor::setup();
    do_spawn(handle, child_spawn_blocking_child);

    executor::exit_handle exit_handle = handle.wait_any();
    require_exit(EXIT_SUCCESS, exit_handle.status());
    exit_handle.cleanup();

    handle.cleanup();

    if (!fs::exists(fs::path("pid")))
        fail("The pid file was not created");

    std::ifstream pidfile("pid");
    ATF_REQUIRE(pidfile);
    pid_t pid;
    pidfile >> pid;
    pidfile.close();

    ensure_dead(pid);
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__prevent_clobbering_control_files);
ATF_TEST_CASE_BODY(integration__prevent_clobbering_control_files)
{
    executor::executor_handle handle = executor::setup();

    do_spawn(handle, child_delete_all);

    executor::exit_handle exit_handle = handle.wait_any();
    require_exit(EXIT_SUCCESS, exit_handle.status());
    ATF_REQUIRE(atf::utils::file_exists(
        (exit_handle.control_directory() / "exec_was_called").str()));
    ATF_REQUIRE(!atf::utils::file_exists(
        (exit_handle.work_directory() / "exec_was_called").str()));
    exit_handle.cleanup();

    handle.cleanup();
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, integration__run_one);
    ATF_ADD_TEST_CASE(tcs, integration__run_many);

    ATF_ADD_TEST_CASE(tcs, integration__parameters_and_output);
    ATF_ADD_TEST_CASE(tcs, integration__custom_output_files);
    ATF_ADD_TEST_CASE(tcs, integration__timestamps);
    ATF_ADD_TEST_CASE(tcs, integration__files);

    ATF_ADD_TEST_CASE(tcs, integration__followup);

    ATF_ADD_TEST_CASE(tcs, integration__output_files_always_exist);
    ATF_ADD_TEST_CASE(tcs, integration__timeouts);
    ATF_ADD_TEST_CASE(tcs, integration__unprivileged_user);
    ATF_ADD_TEST_CASE(tcs, integration__auto_cleanup);
    ATF_ADD_TEST_CASE(tcs, integration__signal_handling);
    ATF_ADD_TEST_CASE(tcs, integration__isolate_child_is_called);
    ATF_ADD_TEST_CASE(tcs, integration__process_group_is_terminated);
    ATF_ADD_TEST_CASE(tcs, integration__prevent_clobbering_control_files);
}

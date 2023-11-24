// Copyright 2012 The Kyua Authors.
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

#include "utils/stacktrace.hpp"

extern "C" {
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <signal.h>
#include <unistd.h>
}

#include <iostream>
#include <sstream>

#include <atf-c++.hpp>

#include "utils/datetime.hpp"
#include "utils/env.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/process/executor.ipp"
#include "utils/process/child.ipp"
#include "utils/process/operations.hpp"
#include "utils/process/status.hpp"
#include "utils/sanity.hpp"
#include "utils/test_utils.ipp"

namespace datetime = utils::datetime;
namespace executor = utils::process::executor;
namespace fs = utils::fs;
namespace process = utils::process;

using utils::none;
using utils::optional;


namespace {


/// Functor to execute a binary in a subprocess.
///
/// The provided binary is copied to the current work directory before being
/// executed and the copy is given the name chosen by the user.  The copy is
/// necessary so that we have a deterministic location for where core files may
/// be dumped (if they happen to be dumped in the current directory).
class crash_me {
    /// Path to the binary to execute.
    const fs::path _binary;

    /// Name of the binary after being copied.
    const fs::path _copy_name;

public:
    /// Constructor.
    ///
    /// \param binary_ Path to binary to execute.
    /// \param copy_name_ Name of the binary after being copied.  If empty,
    ///     use the leaf name of binary_.
    explicit crash_me(const fs::path& binary_,
                      const std::string& copy_name_ = "") :
        _binary(binary_),
        _copy_name(copy_name_.empty() ? binary_.leaf_name() : copy_name_)
    {
    }

    /// Runs the binary.
    void
    operator()(void) const UTILS_NORETURN
    {
        atf::utils::copy_file(_binary.str(), _copy_name.str());

        const std::vector< std::string > args;
        process::exec(_copy_name, args);
    }

    /// Runs the binary.
    ///
    /// This interface is exposed to support passing crash_me to the executor.
    void
    operator()(const fs::path& /* control_directory */) const
        UTILS_NORETURN
    {
        (*this)();  // Delegate to ensure the two entry points remain in sync.
    }
};


static void child_exit(const fs::path&) UTILS_NORETURN;


/// Subprocess that exits cleanly.
static void
child_exit(const fs::path& /* control_directory */)
{
    ::_exit(EXIT_SUCCESS);
}


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


/// Generates a core dump, if possible.
///
/// \post If this fails to generate a core file, the test case is marked as
/// skipped.  The caller can rely on this when attempting further checks on the
/// core dump by assuming that the core dump exists somewhere.
///
/// \param test_case Pointer to the caller test case, needed to obtain the path
///     to the source directory.
/// \param base_name Name of the binary to execute, which will be a copy of a
///     helper binary that always crashes.  This name should later be part of
///     the core filename.
///
/// \return The status of the crashed binary.
static process::status
generate_core(const atf::tests::tc* test_case, const char* base_name)
{
    utils::prepare_coredump_test(test_case);

    const fs::path helper = fs::path(test_case->get_config_var("srcdir")) /
        "stacktrace_helper";

    const process::status status = process::child::fork_files(
        crash_me(helper, base_name),
        fs::path("unused.out"), fs::path("unused.err"))->wait();
    ATF_REQUIRE(status.signaled());
    if (!status.coredump())
        ATF_SKIP("Test failed to generate core dump");
    return status;
}


/// Generates a core dump, if possible.
///
/// \post If this fails to generate a core file, the test case is marked as
/// skipped.  The caller can rely on this when attempting further checks on the
/// core dump by assuming that the core dump exists somewhere.
///
/// \param test_case Pointer to the caller test case, needed to obtain the path
///     to the source directory.
/// \param base_name Name of the binary to execute, which will be a copy of a
///     helper binary that always crashes.  This name should later be part of
///     the core filename.
/// \param executor_handle Executor to use to generate the core dump.
///
/// \return The exit handle of the subprocess so that a stacktrace can be
/// executed reusing this context later on.
static executor::exit_handle
generate_core(const atf::tests::tc* test_case, const char* base_name,
              executor::executor_handle& executor_handle)
{
    utils::prepare_coredump_test(test_case);

    const fs::path helper = fs::path(test_case->get_config_var("srcdir")) /
        "stacktrace_helper";

    const executor::exec_handle exec_handle = executor_handle.spawn(
        crash_me(helper, base_name), datetime::delta(60, 0), none, none, none);
    const executor::exit_handle exit_handle = executor_handle.wait(exec_handle);

    if (!exit_handle.status())
        ATF_SKIP("Test failed to generate core dump (timed out)");
    const process::status& status = exit_handle.status().get();
    ATF_REQUIRE(status.signaled());
    if (!status.coredump())
        ATF_SKIP("Test failed to generate core dump");

    return exit_handle;
}


/// Creates a script.
///
/// \param script Path to the script to create.
/// \param contents Contents of the script.
static void
create_script(const char* script, const std::string& contents)
{
    atf::utils::create_file(script, "#! /bin/sh\n\n" + contents);
    ATF_REQUIRE(::chmod(script, 0755) != -1);
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(unlimit_core_size);
ATF_TEST_CASE_BODY(unlimit_core_size)
{
    utils::require_run_coredump_tests(this);

    struct rlimit rl;
    rl.rlim_cur = 0;
    rl.rlim_max = RLIM_INFINITY;
    if (::setrlimit(RLIMIT_CORE, &rl) == -1)
        skip("Failed to lower the core size limit");

    ATF_REQUIRE(utils::unlimit_core_size());

    const fs::path helper = fs::path(get_config_var("srcdir")) /
        "stacktrace_helper";
    const process::status status = process::child::fork_files(
        crash_me(helper),
        fs::path("unused.out"), fs::path("unused.err"))->wait();
    ATF_REQUIRE(status.signaled());
    if (!status.coredump())
        fail("Core not dumped as expected");
}


ATF_TEST_CASE_WITHOUT_HEAD(unlimit_core_size__hard_is_zero);
ATF_TEST_CASE_BODY(unlimit_core_size__hard_is_zero)
{
    utils::require_run_coredump_tests(this);

    struct rlimit rl;
    rl.rlim_cur = 0;
    rl.rlim_max = 0;
    if (::setrlimit(RLIMIT_CORE, &rl) == -1)
        skip("Failed to lower the core size limit");

    ATF_REQUIRE(!utils::unlimit_core_size());

    const fs::path helper = fs::path(get_config_var("srcdir")) /
        "stacktrace_helper";
    const process::status status = process::child::fork_files(
        crash_me(helper),
        fs::path("unused.out"), fs::path("unused.err"))->wait();
    ATF_REQUIRE(status.signaled());
    ATF_REQUIRE(!status.coredump());
}


ATF_TEST_CASE_WITHOUT_HEAD(find_gdb__use_builtin);
ATF_TEST_CASE_BODY(find_gdb__use_builtin)
{
    utils::builtin_gdb = "/path/to/gdb";
    optional< fs::path > gdb = utils::find_gdb();
    ATF_REQUIRE(gdb);
    ATF_REQUIRE_EQ("/path/to/gdb", gdb.get().str());
}


ATF_TEST_CASE_WITHOUT_HEAD(find_gdb__search_builtin__ok);
ATF_TEST_CASE_BODY(find_gdb__search_builtin__ok)
{
    atf::utils::create_file("custom-name", "");
    ATF_REQUIRE(::chmod("custom-name", 0755) != -1);
    const fs::path exp_gdb = fs::path("custom-name").to_absolute();

    utils::setenv("PATH", "/non-existent/location:.:/bin");

    utils::builtin_gdb = "custom-name";
    optional< fs::path > gdb = utils::find_gdb();
    ATF_REQUIRE(gdb);
    ATF_REQUIRE_EQ(exp_gdb, gdb.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(find_gdb__search_builtin__fail);
ATF_TEST_CASE_BODY(find_gdb__search_builtin__fail)
{
    utils::setenv("PATH", ".");
    utils::builtin_gdb = "foo";
    optional< fs::path > gdb = utils::find_gdb();
    ATF_REQUIRE(!gdb);
}


ATF_TEST_CASE_WITHOUT_HEAD(find_gdb__bogus_value);
ATF_TEST_CASE_BODY(find_gdb__bogus_value)
{
    utils::builtin_gdb = "";
    optional< fs::path > gdb = utils::find_gdb();
    ATF_REQUIRE(!gdb);
}


ATF_TEST_CASE_WITHOUT_HEAD(find_core__found__short);
ATF_TEST_CASE_BODY(find_core__found__short)
{
    const process::status status = generate_core(this, "short");
    INV(status.coredump());
    const optional< fs::path > core_name = utils::find_core(
        fs::path("short"), status, fs::path("."));
    if (!core_name)
        fail("Core dumped, but no candidates found");
    ATF_REQUIRE(core_name.get().str().find("core") != std::string::npos);
    ATF_REQUIRE(fs::exists(core_name.get()));
}


ATF_TEST_CASE_WITHOUT_HEAD(find_core__found__long);
ATF_TEST_CASE_BODY(find_core__found__long)
{
    const process::status status = generate_core(
        this, "long-name-that-may-be-truncated-in-some-systems");
    INV(status.coredump());
    const optional< fs::path > core_name = utils::find_core(
        fs::path("long-name-that-may-be-truncated-in-some-systems"),
        status, fs::path("."));
    if (!core_name)
        fail("Core dumped, but no candidates found");
    ATF_REQUIRE(core_name.get().str().find("core") != std::string::npos);
    ATF_REQUIRE(fs::exists(core_name.get()));
}


ATF_TEST_CASE_WITHOUT_HEAD(find_core__not_found);
ATF_TEST_CASE_BODY(find_core__not_found)
{
    const process::status status = process::status::fake_signaled(SIGILL, true);
    const optional< fs::path > core_name = utils::find_core(
        fs::path("missing"), status, fs::path("."));
    if (core_name)
        fail("Core not dumped, but candidate found: " + core_name.get().str());
}


ATF_TEST_CASE(dump_stacktrace__integration);
ATF_TEST_CASE_HEAD(dump_stacktrace__integration)
{
    set_md_var("require.progs", utils::builtin_gdb);
}
ATF_TEST_CASE_BODY(dump_stacktrace__integration)
{
    executor::executor_handle handle = executor::setup();

    executor::exit_handle exit_handle = generate_core(this, "short", handle);
    INV(exit_handle.status());
    INV(exit_handle.status().get().coredump());

    std::ostringstream output;
    utils::dump_stacktrace(fs::path("short"), handle, exit_handle);

    // It is hard to validate the execution of an arbitrary GDB of which we do
    // not know anything.  Just assume that the backtrace, at the very least,
    // prints a couple of frame identifiers.
    ATF_REQUIRE(!atf::utils::grep_file("#0", exit_handle.stdout_file().str()));
    ATF_REQUIRE( atf::utils::grep_file("#0", exit_handle.stderr_file().str()));
    ATF_REQUIRE(!atf::utils::grep_file("#1", exit_handle.stdout_file().str()));
    ATF_REQUIRE( atf::utils::grep_file("#1", exit_handle.stderr_file().str()));

    exit_handle.cleanup();
    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(dump_stacktrace__ok);
ATF_TEST_CASE_BODY(dump_stacktrace__ok)
{
    utils::setenv("PATH", ".");
    create_script("fake-gdb", "echo 'frame 1'; echo 'frame 2'; "
                  "echo 'some warning' 1>&2; exit 0");
    utils::builtin_gdb = "fake-gdb";

    executor::executor_handle handle = executor::setup();
    executor::exit_handle exit_handle = generate_core(this, "short", handle);
    INV(exit_handle.status());
    INV(exit_handle.status().get().coredump());

    utils::dump_stacktrace(fs::path("short"), handle, exit_handle);

    // Note how all output is expected on stderr even for the messages that the
    // script decided to send to stdout.
    ATF_REQUIRE(atf::utils::grep_file("exited with signal [0-9]* and dumped",
                                      exit_handle.stderr_file().str()));
    ATF_REQUIRE(atf::utils::grep_file("^frame 1$",
                                      exit_handle.stderr_file().str()));
    ATF_REQUIRE(atf::utils::grep_file("^frame 2$",
                                      exit_handle.stderr_file().str()));
    ATF_REQUIRE(atf::utils::grep_file("^some warning$",
                                      exit_handle.stderr_file().str()));
    ATF_REQUIRE(atf::utils::grep_file("GDB exited successfully",
                                      exit_handle.stderr_file().str()));

    exit_handle.cleanup();
    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(dump_stacktrace__cannot_find_core);
ATF_TEST_CASE_BODY(dump_stacktrace__cannot_find_core)
{
    // Make sure we can find a GDB binary so that we don't fail the test for
    // the wrong reason.
    utils::setenv("PATH", ".");
    utils::builtin_gdb = "fake-gdb";
    atf::utils::create_file("fake-gdb", "unused");

    executor::executor_handle handle = executor::setup();
    executor::exit_handle exit_handle = generate_core(this, "short", handle);

    const optional< fs::path > core_name = utils::find_core(
        fs::path("short"),
        exit_handle.status().get(),
        exit_handle.work_directory());
    if (core_name) {
        // This is needed even if we provide a different basename to
        // dump_stacktrace below because the system policies may be generating
        // core dumps by PID, not binary name.
        std::cout << "Removing core dump: " << core_name << '\n';
        fs::unlink(core_name.get());
    }

    utils::dump_stacktrace(fs::path("fake"), handle, exit_handle);

    atf::utils::cat_file(exit_handle.stdout_file().str(), "stdout: ");
    atf::utils::cat_file(exit_handle.stderr_file().str(), "stderr: ");
    ATF_REQUIRE(atf::utils::grep_file("Cannot find any core file",
                                      exit_handle.stderr_file().str()));

    exit_handle.cleanup();
    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(dump_stacktrace__cannot_find_gdb);
ATF_TEST_CASE_BODY(dump_stacktrace__cannot_find_gdb)
{
    utils::setenv("PATH", ".");
    utils::builtin_gdb = "missing-gdb";

    executor::executor_handle handle = executor::setup();
    executor::exit_handle exit_handle = generate_core(this, "short", handle);

    utils::dump_stacktrace(fs::path("fake"), handle, exit_handle);

    ATF_REQUIRE(atf::utils::grep_file(
                    "Cannot find GDB binary; builtin was 'missing-gdb'",
                    exit_handle.stderr_file().str()));

    exit_handle.cleanup();
    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(dump_stacktrace__gdb_fail);
ATF_TEST_CASE_BODY(dump_stacktrace__gdb_fail)
{
    utils::setenv("PATH", ".");
    create_script("fake-gdb", "echo 'foo'; echo 'bar' 1>&2; exit 1");
    const std::string gdb = (fs::current_path() / "fake-gdb").str();
    utils::builtin_gdb = gdb.c_str();

    executor::executor_handle handle = executor::setup();
    executor::exit_handle exit_handle = generate_core(this, "short", handle);

    atf::utils::create_file((exit_handle.work_directory() / "fake.core").str(),
                            "Invalid core file, but not read");
    utils::dump_stacktrace(fs::path("fake"), handle, exit_handle);

    ATF_REQUIRE(atf::utils::grep_file("^foo$",
                                      exit_handle.stderr_file().str()));
    ATF_REQUIRE(atf::utils::grep_file("^bar$",
                                      exit_handle.stderr_file().str()));
    ATF_REQUIRE(atf::utils::grep_file("GDB failed; see output above",
                                      exit_handle.stderr_file().str()));

    exit_handle.cleanup();
    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(dump_stacktrace__gdb_timeout);
ATF_TEST_CASE_BODY(dump_stacktrace__gdb_timeout)
{
    utils::setenv("PATH", ".");
    create_script("fake-gdb", "while :; do sleep 1; done");
    const std::string gdb = (fs::current_path() / "fake-gdb").str();
    utils::builtin_gdb = gdb.c_str();
    utils::gdb_timeout = datetime::delta(1, 0);

    executor::executor_handle handle = executor::setup();
    executor::exit_handle exit_handle = generate_core(this, "short", handle);

    atf::utils::create_file((exit_handle.work_directory() / "fake.core").str(),
                            "Invalid core file, but not read");
    utils::dump_stacktrace(fs::path("fake"), handle, exit_handle);

    ATF_REQUIRE(atf::utils::grep_file("GDB timed out",
                                      exit_handle.stderr_file().str()));

    exit_handle.cleanup();
    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(dump_stacktrace_if_available__append);
ATF_TEST_CASE_BODY(dump_stacktrace_if_available__append)
{
    utils::setenv("PATH", ".");
    create_script("fake-gdb", "echo 'frame 1'; exit 0");
    utils::builtin_gdb = "fake-gdb";

    executor::executor_handle handle = executor::setup();
    executor::exit_handle exit_handle = generate_core(this, "short", handle);

    atf::utils::create_file(exit_handle.stdout_file().str(), "Pre-stdout");
    atf::utils::create_file(exit_handle.stderr_file().str(), "Pre-stderr");

    utils::dump_stacktrace_if_available(fs::path("short"), handle, exit_handle);

    ATF_REQUIRE(atf::utils::grep_file("Pre-stdout",
                                      exit_handle.stdout_file().str()));
    ATF_REQUIRE(atf::utils::grep_file("Pre-stderr",
                                      exit_handle.stderr_file().str()));
    ATF_REQUIRE(atf::utils::grep_file("frame 1",
                                      exit_handle.stderr_file().str()));

    exit_handle.cleanup();
    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(dump_stacktrace_if_available__no_status);
ATF_TEST_CASE_BODY(dump_stacktrace_if_available__no_status)
{
    executor::executor_handle handle = executor::setup();
    const executor::exec_handle exec_handle = handle.spawn(
        child_pause, datetime::delta(0, 100000), none, none, none);
    executor::exit_handle exit_handle = handle.wait(exec_handle);
    INV(!exit_handle.status());

    utils::dump_stacktrace_if_available(fs::path("short"), handle, exit_handle);
    ATF_REQUIRE(atf::utils::compare_file(exit_handle.stdout_file().str(), ""));
    ATF_REQUIRE(atf::utils::compare_file(exit_handle.stderr_file().str(), ""));

    exit_handle.cleanup();
    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(dump_stacktrace_if_available__no_coredump);
ATF_TEST_CASE_BODY(dump_stacktrace_if_available__no_coredump)
{
    executor::executor_handle handle = executor::setup();
    const executor::exec_handle exec_handle = handle.spawn(
        child_exit, datetime::delta(60, 0), none, none, none);
    executor::exit_handle exit_handle = handle.wait(exec_handle);
    INV(exit_handle.status());
    INV(exit_handle.status().get().exited());
    INV(exit_handle.status().get().exitstatus() == EXIT_SUCCESS);

    utils::dump_stacktrace_if_available(fs::path("short"), handle, exit_handle);
    ATF_REQUIRE(atf::utils::compare_file(exit_handle.stdout_file().str(), ""));
    ATF_REQUIRE(atf::utils::compare_file(exit_handle.stderr_file().str(), ""));

    exit_handle.cleanup();
    handle.cleanup();
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, unlimit_core_size);
    ATF_ADD_TEST_CASE(tcs, unlimit_core_size__hard_is_zero);

    ATF_ADD_TEST_CASE(tcs, find_gdb__use_builtin);
    ATF_ADD_TEST_CASE(tcs, find_gdb__search_builtin__ok);
    ATF_ADD_TEST_CASE(tcs, find_gdb__search_builtin__fail);
    ATF_ADD_TEST_CASE(tcs, find_gdb__bogus_value);

    ATF_ADD_TEST_CASE(tcs, find_core__found__short);
    ATF_ADD_TEST_CASE(tcs, find_core__found__long);
    ATF_ADD_TEST_CASE(tcs, find_core__not_found);

    ATF_ADD_TEST_CASE(tcs, dump_stacktrace__integration);
    ATF_ADD_TEST_CASE(tcs, dump_stacktrace__ok);
    ATF_ADD_TEST_CASE(tcs, dump_stacktrace__cannot_find_core);
    ATF_ADD_TEST_CASE(tcs, dump_stacktrace__cannot_find_gdb);
    ATF_ADD_TEST_CASE(tcs, dump_stacktrace__gdb_fail);
    ATF_ADD_TEST_CASE(tcs, dump_stacktrace__gdb_timeout);

    ATF_ADD_TEST_CASE(tcs, dump_stacktrace_if_available__append);
    ATF_ADD_TEST_CASE(tcs, dump_stacktrace_if_available__no_status);
    ATF_ADD_TEST_CASE(tcs, dump_stacktrace_if_available__no_coredump);
}

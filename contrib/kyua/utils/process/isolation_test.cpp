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

#include "utils/process/isolation.hpp"

extern "C" {
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/stat.h>

#include <unistd.h>
}

#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include <atf-c++.hpp>

#include "utils/defs.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/passwd.hpp"
#include "utils/process/child.ipp"
#include "utils/process/status.hpp"
#include "utils/sanity.hpp"
#include "utils/test_utils.ipp"

namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace process = utils::process;

using utils::none;
using utils::optional;


namespace {


/// Runs the given hook in a subprocess.
///
/// \param hook The code to run in the subprocess.
///
/// \return The status of the subprocess for further validation.
///
/// \post The subprocess.stdout and subprocess.stderr files, created in the
/// current directory, contain the output of the subprocess.
template< typename Hook >
static process::status
fork_and_run(Hook hook)
{
    std::unique_ptr< process::child > child = process::child::fork_files(
        hook, fs::path("subprocess.stdout"), fs::path("subprocess.stderr"));
    const process::status status = child->wait();

    atf::utils::cat_file("subprocess.stdout", "isolated child stdout: ");
    atf::utils::cat_file("subprocess.stderr", "isolated child stderr: ");

    return status;
}


/// Subprocess that validates the cleanliness of the environment.
///
/// \post Exits with success if the environment is clean; failure otherwise.
static void
check_clean_environment(void)
{
    fs::mkdir(fs::path("some-directory"), 0755);
    process::isolate_child(none, fs::path("some-directory"));

    bool failed = false;

    const char* empty[] = { "LANG", "LC_ALL", "LC_COLLATE", "LC_CTYPE",
                            "LC_MESSAGES", "LC_MONETARY", "LC_NUMERIC",
                            "LC_TIME", NULL };
    const char** iter;
    for (iter = empty; *iter != NULL; ++iter) {
        if (utils::getenv(*iter)) {
            failed = true;
            std::cout << F("%s was not unset\n") % *iter;
        }
    }

    if (utils::getenv_with_default("HOME", "") != "some-directory") {
        failed = true;
        std::cout << "HOME was not set to the work directory\n";
    }

    if (utils::getenv_with_default("TMPDIR", "") != "some-directory") {
        failed = true;
        std::cout << "TMPDIR was not set to the work directory\n";
    }

    if (utils::getenv_with_default("TZ", "") != "UTC") {
        failed = true;
        std::cout << "TZ was not set to UTC\n";
    }

    if (utils::getenv_with_default("LEAVE_ME_ALONE", "") != "kill-some-day") {
        failed = true;
        std::cout << "LEAVE_ME_ALONE was modified while it should not have "
            "been\n";
    }

    std::exit(failed ? EXIT_FAILURE : EXIT_SUCCESS);
}


/// Subprocess that checks if user privileges are dropped.
class check_drop_privileges {
    /// The user to drop the privileges to.
    const passwd::user _unprivileged_user;

public:
    /// Constructor.
    ///
    /// \param unprivileged_user The user to drop the privileges to.
    check_drop_privileges(const passwd::user& unprivileged_user) :
        _unprivileged_user(unprivileged_user)
    {
    }

    /// Body of the subprocess.
    ///
    /// \post Exits with success if the process has dropped privileges as
    /// expected.
    void
    operator()(void) const
    {
        fs::mkdir(fs::path("subdir"), 0755);
        process::isolate_child(utils::make_optional(_unprivileged_user),
                               fs::path("subdir"));

        if (::getuid() == 0) {
            std::cout << "UID is still 0\n";
            std::exit(EXIT_FAILURE);
        }

        if (::getgid() == 0) {
            std::cout << "GID is still 0\n";
            std::exit(EXIT_FAILURE);
        }

        ::gid_t groups[1];
        if (::getgroups(1, groups) == -1) {
            // Should only fail if we get more than one group notifying about
            // not enough space in the groups variable to store the whole
            // result.
            INV(errno == EINVAL);
            std::exit(EXIT_FAILURE);
        }
        if (groups[0] == 0) {
            std::cout << "Primary group is still 0\n";
            std::exit(EXIT_FAILURE);
        }

        std::ofstream output("file.txt");
        if (!output) {
            std::cout << "Cannot write to isolated directory; owner not "
                "changed?\n";
            std::exit(EXIT_FAILURE);
        }

        std::exit(EXIT_SUCCESS);
    }
};


/// Subprocess that dumps core to validate core dumping abilities.
static void
check_enable_core_dumps(void)
{
    process::isolate_child(none, fs::path("."));
    std::abort();
}


/// Subprocess that checks if the work directory is entered.
class check_enter_work_directory {
    /// Directory to enter.  May be releative.
    const fs::path _directory;

public:
    /// Constructor.
    ///
    /// \param directory Directory to enter.
    check_enter_work_directory(const fs::path& directory) :
        _directory(directory)
    {
    }

    /// Body of the subprocess.
    ///
    /// \post Exits with success if the process has entered the given work
    /// directory; false otherwise.
    void
    operator()(void) const
    {
        const fs::path exp_subdir = fs::current_path() / _directory;
        process::isolate_child(none, _directory);
        std::exit(fs::current_path() == exp_subdir ?
                  EXIT_SUCCESS : EXIT_FAILURE);
    }
};


/// Subprocess that validates that it owns a session.
///
/// \post Exits with success if the process lives in its own session;
/// failure otherwise.
static void
check_new_session(void)
{
    process::isolate_child(none, fs::path("."));
    std::exit(::getsid(::getpid()) == ::getpid() ? EXIT_SUCCESS : EXIT_FAILURE);
}


/// Subprocess that validates the disconnection from any terminal.
///
/// \post Exits with success if the environment is clean; failure otherwise.
static void
check_no_terminal(void)
{
    process::isolate_child(none, fs::path("."));

    const char* const args[] = {
        "/bin/sh",
        "-i",
        "-c",
        "echo success",
        NULL
    };
    ::execv("/bin/sh", UTILS_UNCONST(char*, args));
    std::abort();
}


/// Subprocess that validates that it has become the leader of a process group.
///
/// \post Exits with success if the process lives in its own process group;
/// failure otherwise.
static void
check_process_group(void)
{
    process::isolate_child(none, fs::path("."));
    std::exit(::getpgid(::getpid()) == ::getpid() ?
              EXIT_SUCCESS : EXIT_FAILURE);
}


/// Subprocess that validates that the umask has been reset.
///
/// \post Exits with success if the umask matches the expected value; failure
/// otherwise.
static void
check_umask(void)
{
    process::isolate_child(none, fs::path("."));
    std::exit(::umask(0) == 0022 ? EXIT_SUCCESS : EXIT_FAILURE);
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(isolate_child__clean_environment);
ATF_TEST_CASE_BODY(isolate_child__clean_environment)
{
    utils::setenv("HOME", "/non-existent/directory");
    utils::setenv("TMPDIR", "/non-existent/directory");
    utils::setenv("LANG", "C");
    utils::setenv("LC_ALL", "C");
    utils::setenv("LC_COLLATE", "C");
    utils::setenv("LC_CTYPE", "C");
    utils::setenv("LC_MESSAGES", "C");
    utils::setenv("LC_MONETARY", "C");
    utils::setenv("LC_NUMERIC", "C");
    utils::setenv("LC_TIME", "C");
    utils::setenv("LEAVE_ME_ALONE", "kill-some-day");
    utils::setenv("TZ", "EST+5");

    const process::status status = fork_and_run(check_clean_environment);
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(EXIT_SUCCESS, status.exitstatus());
}


ATF_TEST_CASE(isolate_child__other_user_when_unprivileged);
ATF_TEST_CASE_HEAD(isolate_child__other_user_when_unprivileged)
{
    set_md_var("require.user", "unprivileged");
}
ATF_TEST_CASE_BODY(isolate_child__other_user_when_unprivileged)
{
    const passwd::user user = passwd::current_user();

    passwd::user other_user = user;
    other_user.uid += 1;
    other_user.gid += 1;
    process::isolate_child(utils::make_optional(other_user), fs::path("."));

    ATF_REQUIRE_EQ(user.uid, ::getuid());
    ATF_REQUIRE_EQ(user.gid, ::getgid());
}


ATF_TEST_CASE(isolate_child__drop_privileges);
ATF_TEST_CASE_HEAD(isolate_child__drop_privileges)
{
    set_md_var("require.config", "unprivileged-user");
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(isolate_child__drop_privileges)
{
    const passwd::user unprivileged_user = passwd::find_user_by_name(
        get_config_var("unprivileged-user"));

    const process::status status = fork_and_run(check_drop_privileges(
        unprivileged_user));
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(EXIT_SUCCESS, status.exitstatus());
}


ATF_TEST_CASE(isolate_child__drop_privileges_fail_uid);
ATF_TEST_CASE_HEAD(isolate_child__drop_privileges_fail_uid)
{
    set_md_var("require.user", "unprivileged");
}
ATF_TEST_CASE_BODY(isolate_child__drop_privileges_fail_uid)
{
    // Fake the current user as root so that we bypass the protections in
    // isolate_child that prevent us from attempting a user switch when we are
    // not root.  We do this so we can trigger the setuid failure.
    passwd::user root = passwd::user("root", 0, 0);
    ATF_REQUIRE(root.is_root());
    passwd::set_current_user_for_testing(root);

    passwd::user unprivileged_user = passwd::current_user();
    unprivileged_user.uid += 1;

    const process::status status = fork_and_run(check_drop_privileges(
        unprivileged_user));
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(process::exit_isolation_failure, status.exitstatus());
    ATF_REQUIRE(atf::utils::grep_file("(chown|setuid).*failed",
                                      "subprocess.stderr"));
}


ATF_TEST_CASE(isolate_child__drop_privileges_fail_gid);
ATF_TEST_CASE_HEAD(isolate_child__drop_privileges_fail_gid)
{
    set_md_var("require.user", "unprivileged");
}
ATF_TEST_CASE_BODY(isolate_child__drop_privileges_fail_gid)
{
    // Fake the current user as root so that we bypass the protections in
    // isolate_child that prevent us from attempting a user switch when we are
    // not root.  We do this so we can trigger the setgid failure.
    passwd::user root = passwd::user("root", 0, 0);
    ATF_REQUIRE(root.is_root());
    passwd::set_current_user_for_testing(root);

    passwd::user unprivileged_user = passwd::current_user();
    unprivileged_user.gid += 1;

    const process::status status = fork_and_run(check_drop_privileges(
        unprivileged_user));
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(process::exit_isolation_failure, status.exitstatus());
    ATF_REQUIRE(atf::utils::grep_file("(chown|setgid).*failed",
                                      "subprocess.stderr"));
}


ATF_TEST_CASE_WITHOUT_HEAD(isolate_child__enable_core_dumps);
ATF_TEST_CASE_BODY(isolate_child__enable_core_dumps)
{
    utils::require_run_coredump_tests(this);

    struct ::rlimit rl;
    if (::getrlimit(RLIMIT_CORE, &rl) == -1)
        fail("Failed to query the core size limit");
    if (rl.rlim_cur == 0 || rl.rlim_max == 0)
        skip("Maximum core size is zero; cannot run test");
    rl.rlim_cur = 0;
    if (::setrlimit(RLIMIT_CORE, &rl) == -1)
        fail("Failed to lower the core size limit");

    const process::status status = fork_and_run(check_enable_core_dumps);
    ATF_REQUIRE(status.signaled());
    ATF_REQUIRE(status.coredump());
}


ATF_TEST_CASE_WITHOUT_HEAD(isolate_child__enter_work_directory);
ATF_TEST_CASE_BODY(isolate_child__enter_work_directory)
{
    const fs::path directory("some/sub/directory");
    fs::mkdir_p(directory, 0755);
    const process::status status = fork_and_run(
        check_enter_work_directory(directory));
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(EXIT_SUCCESS, status.exitstatus());
}


ATF_TEST_CASE_WITHOUT_HEAD(isolate_child__enter_work_directory_failure);
ATF_TEST_CASE_BODY(isolate_child__enter_work_directory_failure)
{
    const fs::path directory("some/sub/directory");
    const process::status status = fork_and_run(
        check_enter_work_directory(directory));
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(process::exit_isolation_failure, status.exitstatus());
    ATF_REQUIRE(atf::utils::grep_file("chdir\\(some/sub/directory\\) failed",
                                      "subprocess.stderr"));
}


ATF_TEST_CASE_WITHOUT_HEAD(isolate_child__new_session);
ATF_TEST_CASE_BODY(isolate_child__new_session)
{
    const process::status status = fork_and_run(check_new_session);
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(EXIT_SUCCESS, status.exitstatus());
}


ATF_TEST_CASE_WITHOUT_HEAD(isolate_child__no_terminal);
ATF_TEST_CASE_BODY(isolate_child__no_terminal)
{
    const process::status status = fork_and_run(check_no_terminal);
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(EXIT_SUCCESS, status.exitstatus());
}


ATF_TEST_CASE_WITHOUT_HEAD(isolate_child__process_group);
ATF_TEST_CASE_BODY(isolate_child__process_group)
{
    const process::status status = fork_and_run(check_process_group);
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(EXIT_SUCCESS, status.exitstatus());
}


ATF_TEST_CASE_WITHOUT_HEAD(isolate_child__reset_umask);
ATF_TEST_CASE_BODY(isolate_child__reset_umask)
{
    const process::status status = fork_and_run(check_umask);
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(EXIT_SUCCESS, status.exitstatus());
}


/// Executes isolate_path() and compares the on-disk changes to expected values.
///
/// \param unprivileged_user The user to pass to isolate_path; may be none.
/// \param exp_uid Expected UID or none to expect the old value.
/// \param exp_gid Expected GID or none to expect the old value.
static void
do_isolate_path_test(const optional< passwd::user >& unprivileged_user,
                     const optional< uid_t >& exp_uid,
                     const optional< gid_t >& exp_gid)
{
    const fs::path dir("dir");
    fs::mkdir(dir, 0755);
    struct ::stat old_sb;
    ATF_REQUIRE(::stat(dir.c_str(), &old_sb) != -1);

    process::isolate_path(unprivileged_user, dir);

    struct ::stat new_sb;
    ATF_REQUIRE(::stat(dir.c_str(), &new_sb) != -1);

    if (exp_uid)
        ATF_REQUIRE_EQ(exp_uid.get(), new_sb.st_uid);
    else
        ATF_REQUIRE_EQ(old_sb.st_uid, new_sb.st_uid);

    if (exp_gid)
        ATF_REQUIRE_EQ(exp_gid.get(), new_sb.st_gid);
    else
        ATF_REQUIRE_EQ(old_sb.st_gid, new_sb.st_gid);
}


ATF_TEST_CASE_WITHOUT_HEAD(isolate_path__no_user);
ATF_TEST_CASE_BODY(isolate_path__no_user)
{
    do_isolate_path_test(none, none, none);
}


ATF_TEST_CASE_WITHOUT_HEAD(isolate_path__same_user);
ATF_TEST_CASE_BODY(isolate_path__same_user)
{
    do_isolate_path_test(utils::make_optional(passwd::current_user()),
                         none, none);
}


ATF_TEST_CASE(isolate_path__other_user_when_unprivileged);
ATF_TEST_CASE_HEAD(isolate_path__other_user_when_unprivileged)
{
    set_md_var("require.user", "unprivileged");
}
ATF_TEST_CASE_BODY(isolate_path__other_user_when_unprivileged)
{
    passwd::user user = passwd::current_user();
    user.uid += 1;
    user.gid += 1;

    do_isolate_path_test(utils::make_optional(user), none, none);
}


ATF_TEST_CASE(isolate_path__drop_privileges);
ATF_TEST_CASE_HEAD(isolate_path__drop_privileges)
{
    set_md_var("require.config", "unprivileged-user");
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(isolate_path__drop_privileges)
{
    const passwd::user unprivileged_user = passwd::find_user_by_name(
        get_config_var("unprivileged-user"));
    do_isolate_path_test(utils::make_optional(unprivileged_user),
                         utils::make_optional(unprivileged_user.uid),
                         utils::make_optional(unprivileged_user.gid));
}


ATF_TEST_CASE(isolate_path__drop_privileges_only_uid);
ATF_TEST_CASE_HEAD(isolate_path__drop_privileges_only_uid)
{
    set_md_var("require.config", "unprivileged-user");
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(isolate_path__drop_privileges_only_uid)
{
    passwd::user unprivileged_user = passwd::find_user_by_name(
        get_config_var("unprivileged-user"));
    unprivileged_user.gid = ::getgid();
    do_isolate_path_test(utils::make_optional(unprivileged_user),
                         utils::make_optional(unprivileged_user.uid),
                         none);
}


ATF_TEST_CASE(isolate_path__drop_privileges_only_gid);
ATF_TEST_CASE_HEAD(isolate_path__drop_privileges_only_gid)
{
    set_md_var("require.config", "unprivileged-user");
    set_md_var("require.user", "root");
}
ATF_TEST_CASE_BODY(isolate_path__drop_privileges_only_gid)
{
    passwd::user unprivileged_user = passwd::find_user_by_name(
        get_config_var("unprivileged-user"));
    unprivileged_user.uid = ::getuid();
    do_isolate_path_test(utils::make_optional(unprivileged_user),
                         none,
                         utils::make_optional(unprivileged_user.gid));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, isolate_child__clean_environment);
    ATF_ADD_TEST_CASE(tcs, isolate_child__other_user_when_unprivileged);
    ATF_ADD_TEST_CASE(tcs, isolate_child__drop_privileges);
    ATF_ADD_TEST_CASE(tcs, isolate_child__drop_privileges_fail_uid);
    ATF_ADD_TEST_CASE(tcs, isolate_child__drop_privileges_fail_gid);
    ATF_ADD_TEST_CASE(tcs, isolate_child__enable_core_dumps);
    ATF_ADD_TEST_CASE(tcs, isolate_child__enter_work_directory);
    ATF_ADD_TEST_CASE(tcs, isolate_child__enter_work_directory_failure);
    ATF_ADD_TEST_CASE(tcs, isolate_child__new_session);
    ATF_ADD_TEST_CASE(tcs, isolate_child__no_terminal);
    ATF_ADD_TEST_CASE(tcs, isolate_child__process_group);
    ATF_ADD_TEST_CASE(tcs, isolate_child__reset_umask);

    ATF_ADD_TEST_CASE(tcs, isolate_path__no_user);
    ATF_ADD_TEST_CASE(tcs, isolate_path__same_user);
    ATF_ADD_TEST_CASE(tcs, isolate_path__other_user_when_unprivileged);
    ATF_ADD_TEST_CASE(tcs, isolate_path__drop_privileges);
    ATF_ADD_TEST_CASE(tcs, isolate_path__drop_privileges_only_uid);
    ATF_ADD_TEST_CASE(tcs, isolate_path__drop_privileges_only_gid);
}

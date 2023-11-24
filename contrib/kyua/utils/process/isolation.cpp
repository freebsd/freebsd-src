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
#include <sys/stat.h>

#include <grp.h>
#include <signal.h>
#include <unistd.h>
}

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/env.hpp"
#include "utils/logging/macros.hpp"
#include "utils/optional.ipp"
#include "utils/passwd.hpp"
#include "utils/sanity.hpp"
#include "utils/signals/misc.hpp"
#include "utils/stacktrace.hpp"

namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace process = utils::process;
namespace signals = utils::signals;

using utils::optional;


/// Magic exit code to denote an error while preparing the subprocess.
const int process::exit_isolation_failure = 124;


namespace {


static void fail(const std::string&, const int) UTILS_NORETURN;


/// Fails the process with an errno-based error message.
///
/// \param message The message to print.  The errno-based string will be
///     appended to this, just like in perror(3).
/// \param original_errno The error code to format.
static void
fail(const std::string& message, const int original_errno)
{
    std::cerr << message << ": " << std::strerror(original_errno) << '\n';
    std::exit(process::exit_isolation_failure);
}


/// Changes the owner of a path.
///
/// This function is intended to be called from a subprocess getting ready to
/// invoke an external binary.  Therefore, if there is any error during the
/// setup, the new process is terminated with an error code.
///
/// \param file The path to the file or directory to affect.
/// \param uid The UID to set on the path.
/// \param gid The GID to set on the path.
static void
do_chown(const fs::path& file, const uid_t uid, const gid_t gid)
{
    if (::chown(file.c_str(), uid, gid) == -1)
        fail(F("chown(%s, %s, %s) failed; UID is %s and GID is %s")
             % file % uid % gid % ::getuid() % ::getgid(), errno);
}


/// Resets the environment of the process to a known state.
///
/// \param work_directory Path to the work directory being used.
///
/// \throw std::runtime_error If there is a problem setting up the environment.
static void
prepare_environment(const fs::path& work_directory)
{
    const char* to_unset[] = { "LANG", "LC_ALL", "LC_COLLATE", "LC_CTYPE",
                               "LC_MESSAGES", "LC_MONETARY", "LC_NUMERIC",
                               "LC_TIME", NULL };
    const char** iter;
    for (iter = to_unset; *iter != NULL; ++iter) {
        utils::unsetenv(*iter);
    }

    utils::setenv("HOME", work_directory.str());
    utils::setenv("TMPDIR", work_directory.str());
    utils::setenv("TZ", "UTC");
}


}  // anonymous namespace


/// Cleans up the container process to run a new child.
///
/// If there is any error during the setup, the new process is terminated
/// with an error code.
///
/// \param unprivileged_user Unprivileged user to run the test case as.
/// \param work_directory Path to the test case-specific work directory.
void
process::isolate_child(const optional< passwd::user >& unprivileged_user,
                       const fs::path& work_directory)
{
    isolate_path(unprivileged_user, work_directory);
    if (::chdir(work_directory.c_str()) == -1)
        fail(F("chdir(%s) failed") % work_directory, errno);

    utils::unlimit_core_size();
    if (!signals::reset_all()) {
        LW("Failed to reset one or more signals to their default behavior");
    }
    prepare_environment(work_directory);
    (void)::umask(0022);

    if (unprivileged_user && passwd::current_user().is_root()) {
        const passwd::user& user = unprivileged_user.get();

        if (user.gid != ::getgid()) {
            if (::setgid(user.gid) == -1)
                fail(F("setgid(%s) failed; UID is %s and GID is %s")
                     % user.gid % ::getuid() % ::getgid(), errno);
            if (::getuid() == 0) {
                ::gid_t groups[1];
                groups[0] = user.gid;
                if (::setgroups(1, groups) == -1)
                    fail(F("setgroups(1, [%s]) failed; UID is %s and GID is %s")
                         % user.gid % ::getuid() % ::getgid(), errno);
            }
        }
        if (user.uid != ::getuid()) {
            if (::setuid(user.uid) == -1)
                fail(F("setuid(%s) failed; UID is %s and GID is %s")
                     % user.uid % ::getuid() % ::getgid(), errno);
        }
    }
}


/// Sets up a path to be writable by a child isolated with isolate_child.
///
/// If there is any error during the setup, the new process is terminated
/// with an error code.
///
/// The caller should use this to prepare any directory or file that the child
/// should be able to write to *before* invoking isolate_child().  Note that
/// isolate_child() will use isolate_path() on the work directory though.
///
/// \param unprivileged_user Unprivileged user to run the test case as.
/// \param file Path to the file to modify.
void
process::isolate_path(const optional< passwd::user >& unprivileged_user,
                      const fs::path& file)
{
    if (!unprivileged_user || !passwd::current_user().is_root())
        return;
    const passwd::user& user = unprivileged_user.get();

    const bool change_group = user.gid != ::getgid();
    const bool change_user = user.uid != ::getuid();

    if (!change_user && !change_group) {
        // Keep same permissions.
    } else if (change_user && change_group) {
        do_chown(file, user.uid, user.gid);
    } else if (!change_user && change_group) {
        do_chown(file, ::getuid(), user.gid);
    } else {
        INV(change_user && !change_group);
        do_chown(file, user.uid, ::getgid());
    }
}

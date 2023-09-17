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
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/process/exceptions.hpp"
#include "utils/process/system.hpp"
#include "utils/process/status.hpp"
#include "utils/sanity.hpp"
#include "utils/signals/interrupts.hpp"

namespace fs = utils::fs;
namespace process = utils::process;
namespace signals = utils::signals;


/// Maximum number of arguments supported by exec.
///
/// We need this limit to avoid having to allocate dynamic memory in the child
/// process to construct the arguments list, which would have side-effects in
/// the parent's memory if we use vfork().
#define MAX_ARGS 128


namespace {


/// Exception-based, type-improved version of wait(2).
///
/// \return The PID of the terminated process and its termination status.
///
/// \throw process::system_error If the call to wait(2) fails.
static process::status
safe_wait(void)
{
    LD("Waiting for any child process");
    int stat_loc;
    const pid_t pid = ::wait(&stat_loc);
    if (pid == -1) {
        const int original_errno = errno;
        throw process::system_error("Failed to wait for any child process",
                                    original_errno);
    }
    return process::status(pid, stat_loc);
}


/// Exception-based, type-improved version of waitpid(2).
///
/// \param pid The identifier of the process to wait for.
///
/// \return The termination status of the process.
///
/// \throw process::system_error If the call to waitpid(2) fails.
static process::status
safe_waitpid(const pid_t pid)
{
    LD(F("Waiting for pid=%s") % pid);
    int stat_loc;
    if (process::detail::syscall_waitpid(pid, &stat_loc, 0) == -1) {
        const int original_errno = errno;
        throw process::system_error(F("Failed to wait for PID %s") % pid,
                                    original_errno);
    }
    return process::status(pid, stat_loc);
}


}  // anonymous namespace


/// Executes an external binary and replaces the current process.
///
/// This function must not use any of the logging features so that the output
/// of the subprocess is not "polluted" by our own messages.
///
/// This function must also not affect the global state of the current process
/// as otherwise we would not be able to use vfork().  Only state stored in the
/// stack can be touched.
///
/// \param program The binary to execute.
/// \param args The arguments to pass to the binary, without the program name.
void
process::exec(const fs::path& program, const args_vector& args) throw()
{
    try {
        exec_unsafe(program, args);
    } catch (const system_error& error) {
        // Error message already printed by exec_unsafe.
        std::abort();
    }
}


/// Executes an external binary and replaces the current process.
///
/// This differs from process::exec() in that this function reports errors
/// caused by the exec(2) system call to let the caller decide how to handle
/// them.
///
/// This function must not use any of the logging features so that the output
/// of the subprocess is not "polluted" by our own messages.
///
/// This function must also not affect the global state of the current process
/// as otherwise we would not be able to use vfork().  Only state stored in the
/// stack can be touched.
///
/// \param program The binary to execute.
/// \param args The arguments to pass to the binary, without the program name.
///
/// \throw system_error If the exec(2) call fails.
void
process::exec_unsafe(const fs::path& program, const args_vector& args)
{
    PRE(args.size() < MAX_ARGS);
    int original_errno = 0;
    try {
        const char* argv[MAX_ARGS + 1];

        argv[0] = program.c_str();
        for (args_vector::size_type i = 0; i < args.size(); i++)
            argv[1 + i] = args[i].c_str();
        argv[1 + args.size()] = NULL;

        const int ret = ::execv(program.c_str(),
                                (char* const*)(unsigned long)(const void*)argv);
        original_errno = errno;
        INV(ret == -1);
        std::cerr << "Failed to execute " << program << ": "
                  << std::strerror(original_errno) << "\n";
    } catch (const std::runtime_error& error) {
        std::cerr << "Failed to execute " << program << ": "
                  << error.what() << "\n";
        std::abort();
    } catch (...) {
        std::cerr << "Failed to execute " << program << "; got unexpected "
            "exception during exec\n";
        std::abort();
    }

    // We must do this here to prevent our exception from being caught by the
    // generic handlers above.
    INV(original_errno != 0);
    throw system_error("Failed to execute " + program.str(), original_errno);
}


/// Forcibly kills a process group started by us.
///
/// This function is safe to call from an signal handler context.
///
/// Pretty much all of our subprocesses run in their own process group so that
/// we can terminate them and thier children should we need to.  Because of
/// this, the very first thing our subprocesses do is create a new process group
/// for themselves.
///
/// The implication of the above is that simply issuing a killpg() call on the
/// process group is racy: if the subprocess has not yet had a chance to prepare
/// its own process group, then we will not be killing anything.  To solve this,
/// we must also kill() the process group leader itself, and we must do so after
/// the call to killpg().  Doing this is safe because: 1) the process group must
/// have the same ID as the PID of the process that created it; and 2) we have
/// not yet issued a wait() call so we still own the PID.
///
/// The sideffect of doing what we do here is that the process group leader may
/// receive a signal twice.  But we don't care because we are forcibly
/// terminating the process group and none of the processes can controlledly
/// react to SIGKILL.
///
/// \param pgid PID or process group ID to terminate.
void
process::terminate_group(const int pgid)
{
    (void)::killpg(pgid, SIGKILL);
    (void)::kill(pgid, SIGKILL);
}


/// Terminates the current process reproducing the given status.
///
/// The caller process is abruptly terminated.  In particular, no output streams
/// are flushed, no destructors are called, and no atexit(2) handlers are run.
///
/// \param status The status to "re-deliver" to the caller process.
void
process::terminate_self_with(const status& status)
{
    if (status.exited()) {
        ::_exit(status.exitstatus());
    } else {
        INV(status.signaled());
        (void)::kill(::getpid(), status.termsig());
        UNREACHABLE_MSG(F("Signal %s terminated %s but did not terminate "
                          "ourselves") % status.termsig() % status.dead_pid());
    }
}


/// Blocks to wait for completion of a subprocess.
///
/// \param pid Identifier of the process to wait for.
///
/// \return The termination status of the child process that terminated.
///
/// \throw process::system_error If the call to wait(2) fails.
process::status
process::wait(const int pid)
{
    const process::status status = safe_waitpid(pid);
    {
        signals::interrupts_inhibiter inhibiter;
        signals::remove_pid_to_kill(pid);
    }
    return status;
}


/// Blocks to wait for completion of any subprocess.
///
/// \return The termination status of the child process that terminated.
///
/// \throw process::system_error If the call to wait(2) fails.
process::status
process::wait_any(void)
{
    const process::status status = safe_wait();
    {
        signals::interrupts_inhibiter inhibiter;
        signals::remove_pid_to_kill(status.dead_pid());
    }
    return status;
}

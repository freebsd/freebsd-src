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

#include "utils/process/child.ipp"

extern "C" {
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
}

#include <cerrno>
#include <iostream>
#include <memory>

#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/noncopyable.hpp"
#include "utils/process/exceptions.hpp"
#include "utils/process/fdstream.hpp"
#include "utils/process/operations.hpp"
#include "utils/process/system.hpp"
#include "utils/process/status.hpp"
#include "utils/sanity.hpp"
#include "utils/signals/interrupts.hpp"


namespace utils {
namespace process {


/// Private implementation fields for child objects.
struct child::impl : utils::noncopyable {
    /// The process identifier.
    pid_t _pid;

    /// The input stream for the process' stdout and stderr.  May be NULL.
    std::unique_ptr< process::ifdstream > _output;

    /// Initializes private implementation data.
    ///
    /// \param pid The process identifier.
    /// \param output The input stream.  Grabs ownership of the pointer.
    impl(const pid_t pid, process::ifdstream* output) :
        _pid(pid), _output(output) {}
};


}  // namespace process
}  // namespace utils


namespace fs = utils::fs;
namespace process = utils::process;
namespace signals = utils::signals;


namespace {


/// Exception-based version of dup(2).
///
/// \param old_fd The file descriptor to duplicate.
/// \param new_fd The file descriptor to use as the duplicate.  This is
///     closed if it was open before the copy happens.
///
/// \throw process::system_error If the call to dup2(2) fails.
static void
safe_dup(const int old_fd, const int new_fd)
{
    if (process::detail::syscall_dup2(old_fd, new_fd) == -1) {
        const int original_errno = errno;
        throw process::system_error(F("dup2(%s, %s) failed") % old_fd % new_fd,
                                    original_errno);
    }
}


/// Exception-based version of open(2) to open (or create) a file for append.
///
/// \param filename The file to open in append mode.
///
/// \return The file descriptor for the opened or created file.
///
/// \throw process::system_error If the call to open(2) fails.
static int
open_for_append(const fs::path& filename)
{
    const int fd = process::detail::syscall_open(
        filename.c_str(), O_CREAT | O_WRONLY | O_APPEND,
        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1) {
        const int original_errno = errno;
        throw process::system_error(F("Failed to create %s because open(2) "
                                      "failed") % filename, original_errno);
    }
    return fd;
}


/// Logs the execution of another program.
///
/// \param program The binary to execute.
/// \param args The arguments to pass to the binary, without the program name.
static void
log_exec(const fs::path& program, const process::args_vector& args)
{
    std::string plain_command = program.str();
    for (process::args_vector::const_iterator iter = args.begin();
         iter != args.end(); ++iter)
        plain_command += F(" %s") % *iter;
    LD(F("Executing %s") % plain_command);
}


}  // anonymous namespace


/// Prints out a fatal error and aborts.
void
utils::process::detail::report_error_and_abort(void)
{
    std::cerr << "Caught unknown exception\n";
    std::abort();
}


/// Prints out a fatal error and aborts.
///
/// \param error The error to display.
void
utils::process::detail::report_error_and_abort(const std::runtime_error& error)
{
    std::cerr << "Caught runtime_error: " << error.what() << '\n';
    std::abort();
}


/// Creates a new child.
///
/// \param implptr A dynamically-allocated impl object with the contents of the
///     new child.
process::child::child(impl *implptr) :
    _pimpl(implptr)
{
}


/// Destructor for child.
process::child::~child(void)
{
}


/// Helper function for fork().
///
/// Please note: if you update this function to change the return type or to
/// raise different errors, do not forget to update fork() accordingly.
///
/// \return In the case of the parent, a new child object returned as a
/// dynamically-allocated object because children classes are unique and thus
/// noncopyable.  In the case of the child, a NULL pointer.
///
/// \throw process::system_error If the calls to pipe(2) or fork(2) fail.
std::unique_ptr< process::child >
process::child::fork_capture_aux(void)
{
    std::cout.flush();
    std::cerr.flush();

    int fds[2];
    if (detail::syscall_pipe(fds) == -1)
        throw process::system_error("pipe(2) failed", errno);

    std::unique_ptr< signals::interrupts_inhibiter > inhibiter(
        new signals::interrupts_inhibiter);
    pid_t pid = detail::syscall_fork();
    if (pid == -1) {
        inhibiter.reset();  // Unblock signals.
        ::close(fds[0]);
        ::close(fds[1]);
        throw process::system_error("fork(2) failed", errno);
    } else if (pid == 0) {
        inhibiter.reset();  // Unblock signals.
        ::setsid();

        try {
            ::close(fds[0]);
            safe_dup(fds[1], STDOUT_FILENO);
            safe_dup(fds[1], STDERR_FILENO);
            ::close(fds[1]);
        } catch (const system_error& e) {
            std::cerr << F("Failed to set up subprocess: %s\n") % e.what();
            std::abort();
        }
        return {};
    } else {
        ::close(fds[1]);
        LD(F("Spawned process %s: stdout and stderr inherited") % pid);
        signals::add_pid_to_kill(pid);
        inhibiter.reset(NULL);  // Unblock signals.
        return std::unique_ptr< process::child >(
            new process::child(new impl(pid, new process::ifdstream(fds[0]))));
    }
}


/// Helper function for fork().
///
/// Please note: if you update this function to change the return type or to
/// raise different errors, do not forget to update fork() accordingly.
///
/// \param stdout_file The name of the file in which to store the stdout.
///     If this has the magic value /dev/stdout, then the parent's stdout is
///     reused without applying any redirection.
/// \param stderr_file The name of the file in which to store the stderr.
///     If this has the magic value /dev/stderr, then the parent's stderr is
///     reused without applying any redirection.
///
/// \return In the case of the parent, a new child object returned as a
/// dynamically-allocated object because children classes are unique and thus
/// noncopyable.  In the case of the child, a NULL pointer.
///
/// \throw process::system_error If the call to fork(2) fails.
std::unique_ptr< process::child >
process::child::fork_files_aux(const fs::path& stdout_file,
                               const fs::path& stderr_file)
{
    std::cout.flush();
    std::cerr.flush();

    std::unique_ptr< signals::interrupts_inhibiter > inhibiter(
        new signals::interrupts_inhibiter);
    pid_t pid = detail::syscall_fork();
    if (pid == -1) {
        inhibiter.reset();  // Unblock signals.
        throw process::system_error("fork(2) failed", errno);
    } else if (pid == 0) {
        inhibiter.reset();  // Unblock signals.
        ::setsid();

        try {
            if (stdout_file != fs::path("/dev/stdout")) {
                const int stdout_fd = open_for_append(stdout_file);
                safe_dup(stdout_fd, STDOUT_FILENO);
                ::close(stdout_fd);
            }
            if (stderr_file != fs::path("/dev/stderr")) {
                const int stderr_fd = open_for_append(stderr_file);
                safe_dup(stderr_fd, STDERR_FILENO);
                ::close(stderr_fd);
            }
        } catch (const system_error& e) {
            std::cerr << F("Failed to set up subprocess: %s\n") % e.what();
            std::abort();
        }
        return {};
    } else {
        LD(F("Spawned process %s: stdout=%s, stderr=%s") % pid % stdout_file %
           stderr_file);
        signals::add_pid_to_kill(pid);
        inhibiter.reset();  // Unblock signals.
        return std::unique_ptr< process::child >(
            new process::child(new impl(pid, NULL)));
    }
}


/// Spawns a new binary and multiplexes and captures its stdout and stderr.
///
/// If the subprocess cannot be completely set up for any reason, it attempts to
/// dump an error message to its stderr channel and it then calls std::abort().
///
/// \param program The binary to execute.
/// \param args The arguments to pass to the binary, without the program name.
///
/// \return A new child object, returned as a dynamically-allocated object
/// because children classes are unique and thus noncopyable.
///
/// \throw process::system_error If the process cannot be spawned due to a
///     system call error.
std::unique_ptr< process::child >
process::child::spawn_capture(const fs::path& program, const args_vector& args)
{
    std::unique_ptr< child > child = fork_capture_aux();
    if (child.get() == NULL)
        exec(program, args);
    log_exec(program, args);
    return child;
}


/// Spawns a new binary and redirects its stdout and stderr to files.
///
/// If the subprocess cannot be completely set up for any reason, it attempts to
/// dump an error message to its stderr channel and it then calls std::abort().
///
/// \param program The binary to execute.
/// \param args The arguments to pass to the binary, without the program name.
/// \param stdout_file The name of the file in which to store the stdout.
/// \param stderr_file The name of the file in which to store the stderr.
///
/// \return A new child object, returned as a dynamically-allocated object
/// because children classes are unique and thus noncopyable.
///
/// \throw process::system_error If the process cannot be spawned due to a
///     system call error.
std::unique_ptr< process::child >
process::child::spawn_files(const fs::path& program,
                            const args_vector& args,
                            const fs::path& stdout_file,
                            const fs::path& stderr_file)
{
    std::unique_ptr< child > child = fork_files_aux(stdout_file, stderr_file);
    if (child.get() == NULL)
        exec(program, args);
    log_exec(program, args);
    return child;
}


/// Returns the process identifier of this child.
///
/// \return A process identifier.
int
process::child::pid(void) const
{
    return _pimpl->_pid;
}


/// Gets the input stream corresponding to the stdout and stderr of the child.
///
/// \pre The child must have been started by fork_capture().
///
/// \return A reference to the input stream connected to the output of the test
/// case.
std::istream&
process::child::output(void)
{
    PRE(_pimpl->_output.get() != NULL);
    return *_pimpl->_output;
}


/// Blocks to wait for completion.
///
/// \return The termination status of the child process.
///
/// \throw process::system_error If the call to waitpid(2) fails.
process::status
process::child::wait(void)
{
    return process::wait(_pimpl->_pid);
}

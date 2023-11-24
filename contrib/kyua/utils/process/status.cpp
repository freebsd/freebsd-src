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

#include "utils/process/status.hpp"

extern "C" {
#include <sys/wait.h>
}

#include "utils/format/macros.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"

namespace process = utils::process;

using utils::none;
using utils::optional;

#if !defined(WCOREDUMP)
#   define WCOREDUMP(x) false
#endif


/// Constructs a new status object based on the status value of waitpid(2).
///
/// \param dead_pid_ The PID of the process this status belonged to.
/// \param stat_loc The status value returnd by waitpid(2).
process::status::status(const int dead_pid_, int stat_loc) :
    _dead_pid(dead_pid_),
    _exited(WIFEXITED(stat_loc) ?
            optional< int >(WEXITSTATUS(stat_loc)) : none),
    _signaled(WIFSIGNALED(stat_loc) ?
              optional< std::pair< int, bool > >(
                  std::make_pair(WTERMSIG(stat_loc), WCOREDUMP(stat_loc))) :
                  none)
{
}


/// Constructs a new status object based on fake values.
///
/// \param exited_ If not none, specifies the exit status of the program.
/// \param signaled_ If not none, specifies the termination signal and whether
///     the process dumped core or not.
process::status::status(const optional< int >& exited_,
                        const optional< std::pair< int, bool > >& signaled_) :
    _dead_pid(-1),
    _exited(exited_),
    _signaled(signaled_)
{
}


/// Constructs a new status object based on a fake exit status.
///
/// \param exitstatus_ The exit code of the process.
///
/// \return A status object with fake data.
process::status
process::status::fake_exited(const int exitstatus_)
{
    return status(utils::make_optional(exitstatus_), none);
}


/// Constructs a new status object based on a fake exit status.
///
/// \param termsig_ The termination signal of the process.
/// \param coredump_ Whether the process dumped core or not.
///
/// \return A status object with fake data.
process::status
process::status::fake_signaled(const int termsig_, const bool coredump_)
{
    return status(none, utils::make_optional(std::make_pair(termsig_,
                                                            coredump_)));
}


/// Returns the PID of the process this status was taken from.
///
/// Please note that the process is already dead and gone from the system.  This
/// PID can only be used for informational reasons and not to address the
/// process in any way.
///
/// \return The PID of the original process.
int
process::status::dead_pid(void) const
{
    return _dead_pid;
}


/// Returns whether the process exited cleanly or not.
///
/// \return True if the process exited cleanly, false otherwise.
bool
process::status::exited(void) const
{
    return _exited;
}


/// Returns the exit code of the process.
///
/// \pre The process must have exited cleanly (i.e. exited() must be true).
///
/// \return The exit code.
int
process::status::exitstatus(void) const
{
    PRE(exited());
    return _exited.get();
}


/// Returns whether the process terminated due to a signal or not.
///
/// \return True if the process terminated due to a signal, false otherwise.
bool
process::status::signaled(void) const
{
    return _signaled;
}


/// Returns the signal that terminated the process.
///
/// \pre The process must have terminated by a signal (i.e. signaled() must be
///     true.
///
/// \return The signal number.
int
process::status::termsig(void) const
{
    PRE(signaled());
    return _signaled.get().first;
}


/// Returns whether the process core dumped or not.
///
/// This functionality may be unsupported in some platforms.  In such cases,
/// this method returns false unconditionally.
///
/// \pre The process must have terminated by a signal (i.e. signaled() must be
///     true.
///
/// \return True if the process dumped core, false otherwise.
bool
process::status::coredump(void) const
{
    PRE(signaled());
    return _signaled.get().second;
}


/// Injects the object into a stream.
///
/// \param output The stream into which to inject the object.
/// \param status The object to format.
///
/// \return The output stream.
std::ostream&
process::operator<<(std::ostream& output, const status& status)
{
    if (status.exited()) {
        output << F("status{exitstatus=%s}") % status.exitstatus();
    } else {
        INV(status.signaled());
        output << F("status{termsig=%s, coredump=%s}") % status.termsig() %
            status.coredump();
    }
    return output;
}

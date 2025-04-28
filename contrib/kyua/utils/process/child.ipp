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

#if !defined(UTILS_PROCESS_CHILD_IPP)
#define UTILS_PROCESS_CHILD_IPP

#include <cstdlib>

#include "utils/process/child.hpp"

namespace utils {
namespace process {


/// Spawns a new subprocess and redirects its stdout and stderr to files.
///
/// If the subprocess cannot be completely set up for any reason, it attempts to
/// dump an error message to its stderr channel and it then calls std::abort().
///
/// \param hook The function to execute in the subprocess.  Must not return.
/// \param stdout_file The name of the file in which to store the stdout.
/// \param stderr_file The name of the file in which to store the stderr.
///
/// \return A new child object, returned as a dynamically-allocated object
/// because children classes are unique and thus noncopyable.
///
/// \throw process::system_error If the process cannot be spawned due to a
///     system call error.
template< typename Hook >
std::unique_ptr< child >
child::fork_files(Hook hook, const fs::path& stdout_file,
                  const fs::path& stderr_file)
{
    std::unique_ptr< child > child = fork_files_aux(stdout_file, stderr_file);
    if (child.get() == NULL) {
        try {
            hook();
            std::abort();
        } catch (const std::runtime_error& e) {
            detail::report_error_and_abort(e);
        } catch (...) {
            detail::report_error_and_abort();
        }
    }

    return child;
}


/// Spawns a new subprocess and multiplexes and captures its stdout and stderr.
///
/// If the subprocess cannot be completely set up for any reason, it attempts to
/// dump an error message to its stderr channel and it then calls std::abort().
///
/// \param hook The function to execute in the subprocess.  Must not return.
///
/// \return A new child object, returned as a dynamically-allocated object
/// because children classes are unique and thus noncopyable.
///
/// \throw process::system_error If the process cannot be spawned due to a
///     system call error.
template< typename Hook >
std::unique_ptr< child >
child::fork_capture(Hook hook)
{
    std::unique_ptr< child > child = fork_capture_aux();
    if (child.get() == NULL) {
        try {
            hook();
            std::abort();
        } catch (const std::runtime_error& e) {
            detail::report_error_and_abort(e);
        } catch (...) {
            detail::report_error_and_abort();
        }
    }

    return child;
}


}  // namespace process
}  // namespace utils

#endif  // !defined(UTILS_PROCESS_CHILD_IPP)

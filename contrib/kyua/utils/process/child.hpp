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

/// \file utils/process/child.hpp
/// Spawning and manipulation of children processes.
///
/// The child module provides a set of functions to spawn subprocesses with
/// different settings, and the corresponding set of classes to interact with
/// said subprocesses.  The interfaces to fork subprocesses are very simplified
/// and only provide the minimum functionality required by the rest of the
/// project.
///
/// Be aware that the semantics of the fork and wait methods exposed by this
/// module are slightly different from that of the native calls.  Any process
/// spawned by fork here will be isolated in its own session; once any of
/// such children processes is awaited for, its whole process group will be
/// terminated.  This is the semantics we want in the above layers to ensure
/// that test programs (and, for that matter, external utilities) do not leak
/// subprocesses on the system.

#if !defined(UTILS_PROCESS_CHILD_HPP)
#define UTILS_PROCESS_CHILD_HPP

#include "utils/process/child_fwd.hpp"

#include <istream>
#include <memory>
#include <stdexcept>

#include "utils/defs.hpp"
#include "utils/fs/path_fwd.hpp"
#include "utils/noncopyable.hpp"
#include "utils/process/operations_fwd.hpp"
#include "utils/process/status_fwd.hpp"

namespace utils {
namespace process {


namespace detail {

void report_error_and_abort(void) UTILS_NORETURN;
void report_error_and_abort(const std::runtime_error&) UTILS_NORETURN;


}  // namespace detail


/// Child process spawner and controller.
class child : noncopyable {
    struct impl;

    /// Pointer to the shared internal implementation.
    std::unique_ptr< impl > _pimpl;

    static std::unique_ptr< child > fork_capture_aux(void);

    static std::unique_ptr< child > fork_files_aux(const fs::path&,
                                                 const fs::path&);

    explicit child(impl *);

public:
    ~child(void);

    template< typename Hook >
    static std::unique_ptr< child > fork_capture(Hook);
    std::istream& output(void);

    template< typename Hook >
    static std::unique_ptr< child > fork_files(Hook, const fs::path&,
                                             const fs::path&);

    static std::unique_ptr< child > spawn_capture(
        const fs::path&, const args_vector&);
    static std::unique_ptr< child > spawn_files(
        const fs::path&, const args_vector&, const fs::path&, const fs::path&);

    int pid(void) const;

    status wait(void);
};


}  // namespace process
}  // namespace utils

#endif  // !defined(UTILS_PROCESS_CHILD_HPP)

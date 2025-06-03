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

/// \file utils/process/executor.hpp
/// Multiprogrammed process executor with isolation guarantees.
///
/// This module provides a mechanism to invoke more than one process
/// concurrently while at the same time ensuring that each process is run
/// in a clean container and in a "safe" work directory that gets cleaned
/// up automatically on termination.
///
/// The intended workflow for using this module is the following:
///
/// 1) Initialize the executor using setup().  Keep the returned object
///    around through the lifetime of the next operations.  Only one
///    instance of the executor can be alive at once.
/// 2) Spawn one or more processes with spawn().  On the caller side, keep
///    track of any per-process data you may need using the returned
///    exec_handle, which is unique among the set of active processes.
/// 3) Call wait() or wait_any() to wait for completion of a process started
///    in the previous step.  Repeat as desired.
/// 4) Use the returned exit_handle object by wait() or wait_any() to query
///    the status of the terminated process and/or to access any of its
///    data files.
/// 5) Invoke cleanup() on the exit_handle to wipe any stale data.
/// 6) Invoke cleanup() on the object returned by setup().
///
/// It is the responsibility of the caller to ensure that calls to
/// spawn() and spawn_followup() are balanced with wait() and wait_any() calls.
///
/// Processes executed in this manner have access to two different "unique"
/// directories: the first is the "work directory", which is an empty directory
/// that acts as the subprocess' work directory; the second is the "control
/// directory", which is the location where the in-process code may place files
/// that are not clobbered by activities in the work directory.

#if !defined(UTILS_PROCESS_EXECUTOR_HPP)
#define UTILS_PROCESS_EXECUTOR_HPP

#include "utils/process/executor_fwd.hpp"

#include <cstddef>
#include <memory>

#include "utils/datetime_fwd.hpp"
#include "utils/fs/path_fwd.hpp"
#include "utils/optional.hpp"
#include "utils/passwd_fwd.hpp"
#include "utils/process/child_fwd.hpp"
#include "utils/process/status_fwd.hpp"

namespace utils {
namespace process {
namespace executor {


namespace detail {


extern const char* stdout_name;
extern const char* stderr_name;
extern const char* work_subdir;


/// Shared reference counter.
typedef std::shared_ptr< std::size_t > refcnt_t;


void setup_child(const utils::optional< utils::passwd::user >,
                 const utils::fs::path&, const utils::fs::path&);


}   // namespace detail


/// Maintenance data held while a subprocess is being executed.
///
/// This data structure exists from the moment a subprocess is executed via
/// executor::spawn() to when it is cleaned up with exit_handle::cleanup().
///
/// The caller NEED NOT maintain this object alive for the execution of the
/// subprocess.  However, the PID contained in here can be used to match
/// exec_handle objects with corresponding exit_handle objects via their
/// original_pid() method.
///
/// Objects of this type can be copied around but their implementation is
/// shared.  The implication of this is that only the last copy of a given exit
/// handle will execute the automatic cleanup() on destruction.
class exec_handle {
    struct impl;

    /// Pointer to internal implementation.
    std::shared_ptr< impl > _pimpl;

    friend class executor_handle;
    exec_handle(std::shared_ptr< impl >);

public:
    ~exec_handle(void);

    int pid(void) const;
    utils::fs::path control_directory(void) const;
    utils::fs::path work_directory(void) const;
    const utils::fs::path& stdout_file(void) const;
    const utils::fs::path& stderr_file(void) const;
};


/// Container for the data of a process termination.
///
/// This handle provides access to the details of the process that terminated
/// and serves as the owner of the remaining on-disk files.  The caller is
/// expected to call cleanup() before destruction to remove the on-disk state.
///
/// Objects of this type can be copied around but their implementation is
/// shared.  The implication of this is that only the last copy of a given exit
/// handle will execute the automatic cleanup() on destruction.
class exit_handle {
    struct impl;

    /// Pointer to internal implementation.
    std::shared_ptr< impl > _pimpl;

    friend class executor_handle;
    exit_handle(std::shared_ptr< impl >);

    detail::refcnt_t state_owners(void) const;

public:
    ~exit_handle(void);

    void cleanup(void);

    int original_pid(void) const;
    const utils::optional< utils::process::status >& status(void) const;
    const utils::optional< utils::passwd::user >& unprivileged_user(void) const;
    const utils::datetime::timestamp& start_time() const;
    const utils::datetime::timestamp& end_time() const;
    utils::fs::path control_directory(void) const;
    utils::fs::path work_directory(void) const;
    const utils::fs::path& stdout_file(void) const;
    const utils::fs::path& stderr_file(void) const;
};


/// Handler for the livelihood of the executor.
///
/// Objects of this type can be copied around (because we do not have move
/// semantics...) but their implementation is shared.  Only one instance of the
/// executor can exist at any point in time.
class executor_handle {
    struct impl;
    /// Pointer to internal implementation.
    std::shared_ptr< impl > _pimpl;

    friend executor_handle setup(void);
    executor_handle(void) throw();

    utils::fs::path spawn_pre(void);
    exec_handle spawn_post(const utils::fs::path&,
                           const utils::fs::path&,
                           const utils::fs::path&,
                           const utils::datetime::delta&,
                           const utils::optional< utils::passwd::user >,
                           std::unique_ptr< utils::process::child >);

    void spawn_followup_pre(void);
    exec_handle spawn_followup_post(const exit_handle&,
                                    const utils::datetime::delta&,
                                    std::unique_ptr< utils::process::child >);

public:
    ~executor_handle(void);

    const utils::fs::path& root_work_directory(void) const;

    void cleanup(void);

    template< class Hook >
    exec_handle spawn(Hook,
                      const datetime::delta&,
                      const utils::optional< utils::passwd::user >,
                      const utils::optional< utils::fs::path > = utils::none,
                      const utils::optional< utils::fs::path > = utils::none);

    template< class Hook >
    exec_handle spawn_followup(Hook,
                               const exit_handle&,
                               const datetime::delta&);

    exit_handle wait(const exec_handle);
    exit_handle wait_any(void);
    exit_handle reap(const pid_t);

    void check_interrupt(void) const;
};


executor_handle setup(void);


}  // namespace executor
}  // namespace process
}  // namespace utils


#endif  // !defined(UTILS_PROCESS_EXECUTOR_HPP)

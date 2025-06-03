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

#if !defined(UTILS_PROCESS_EXECUTOR_IPP)
#define UTILS_PROCESS_EXECUTOR_IPP

#include "utils/process/executor.hpp"

#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/passwd.hpp"
#include "utils/process/child.ipp"

namespace utils {
namespace process {


namespace executor {
namespace detail {

/// Functor to execute a hook in a child process.
///
/// The hook is executed after the process has been isolated per the logic in
/// utils::process::isolation based on the input parameters at construction
/// time.
template< class Hook >
class run_child {
    /// Function or functor to invoke in the child.
    Hook _hook;

    /// Directory where the hook may place control files.
    const fs::path& _control_directory;

    /// Directory to enter when running the subprocess.
    ///
    /// This is a subdirectory of _control_directory but is separate so that
    /// subprocess operations do not inadvertently affect our files.
    const fs::path& _work_directory;

    /// User to switch to when running the subprocess.
    ///
    /// If not none, the subprocess will be executed as the provided user and
    /// the control and work directories will be writable by this user.
    const optional< passwd::user > _unprivileged_user;

public:
    /// Constructor.
    ///
    /// \param hook Function or functor to invoke in the child.
    /// \param control_directory Directory where control files can be placed.
    /// \param work_directory Directory to enter when running the subprocess.
    /// \param unprivileged_user If set, user to switch to before execution.
    run_child(Hook hook,
              const fs::path& control_directory,
              const fs::path& work_directory,
              const optional< passwd::user > unprivileged_user) :
        _hook(hook),
        _control_directory(control_directory),
        _work_directory(work_directory),
        _unprivileged_user(unprivileged_user)
    {
    }

    /// Body of the subprocess.
    void
    operator()(void)
    {
        executor::detail::setup_child(_unprivileged_user,
                                      _control_directory, _work_directory);
        _hook(_control_directory);
    }
};

}  // namespace detail
}  // namespace executor


/// Forks and executes a subprocess asynchronously.
///
/// \tparam Hook Type of the hook.
/// \param hook Function or functor to run in the subprocess.
/// \param timeout Maximum amount of time the subprocess can run for.
/// \param unprivileged_user If not none, user to switch to before execution.
/// \param stdout_target If not none, file to which to write the stdout of the
///     test case.
/// \param stderr_target If not none, file to which to write the stderr of the
///     test case.
///
/// \return A handle for the background operation.  Used to match the result of
/// the execution returned by wait_any() with this invocation.
template< class Hook >
executor::exec_handle
executor::executor_handle::spawn(
    Hook hook,
    const datetime::delta& timeout,
    const optional< passwd::user > unprivileged_user,
    const optional< fs::path > stdout_target,
    const optional< fs::path > stderr_target)
{
    const fs::path unique_work_directory = spawn_pre();

    const fs::path stdout_path = stdout_target ?
        stdout_target.get() : (unique_work_directory / detail::stdout_name);
    const fs::path stderr_path = stderr_target ?
        stderr_target.get() : (unique_work_directory / detail::stderr_name);

    std::unique_ptr< process::child > child = process::child::fork_files(
        detail::run_child< Hook >(hook,
                                  unique_work_directory,
                                  unique_work_directory / detail::work_subdir,
                                  unprivileged_user),
        stdout_path, stderr_path);

    return spawn_post(unique_work_directory, stdout_path, stderr_path,
                      timeout, unprivileged_user, std::move(child));
}


/// Forks and executes a subprocess asynchronously in the context of another.
///
/// By context we understand the on-disk state of a previously-executed process,
/// thus the new subprocess spawned by this function will run with the same
/// control and work directories as another process.
///
/// \tparam Hook Type of the hook.
/// \param hook Function or functor to run in the subprocess.
/// \param base Context of the subprocess in which to run this one.  The
///     exit_handle provided here must remain alive throughout the existence of
///     this other object because the original exit_handle is the one that owns
///     the on-disk state.
/// \param timeout Maximum amount of time the subprocess can run for.
///
/// \return A handle for the background operation.  Used to match the result of
/// the execution returned by wait_any() with this invocation.
template< class Hook >
executor::exec_handle
executor::executor_handle::spawn_followup(Hook hook,
                                          const exit_handle& base,
                                          const datetime::delta& timeout)
{
    spawn_followup_pre();

    std::unique_ptr< process::child > child = process::child::fork_files(
        detail::run_child< Hook >(hook,
                                  base.control_directory(),
                                  base.work_directory(),
                                  base.unprivileged_user()),
        base.stdout_file(), base.stderr_file());

    return spawn_followup_post(base, timeout, std::move(child));
}


}  // namespace process
}  // namespace utils

#endif  // !defined(UTILS_PROCESS_EXECUTOR_IPP)

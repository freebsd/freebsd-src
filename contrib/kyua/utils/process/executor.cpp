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

#include "utils/process/executor.ipp"

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

extern "C" {
#include <sys/types.h>
#include <sys/wait.h>

#include <signal.h>
}

#include <forward_list>
#include <fstream>
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>

#include "utils/datetime.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/auto_cleaners.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/logging/operations.hpp"
#include "utils/noncopyable.hpp"
#include "utils/optional.ipp"
#include "utils/passwd.hpp"
#include "utils/process/child.ipp"
#include "utils/process/deadline_killer.hpp"
#include "utils/process/isolation.hpp"
#include "utils/process/operations.hpp"
#include "utils/process/status.hpp"
#include "utils/sanity.hpp"
#include "utils/signals/interrupts.hpp"
#include "utils/signals/timer.hpp"

namespace datetime = utils::datetime;
namespace executor = utils::process::executor;
namespace fs = utils::fs;
namespace logging = utils::logging;
namespace passwd = utils::passwd;
namespace process = utils::process;
namespace signals = utils::signals;

using utils::none;
using utils::optional;


namespace {


/// Template for temporary directories created by the executor.
static const char* work_directory_template = PACKAGE_TARNAME ".XXXXXX";


/// Mapping of active subprocess PIDs to their execution data.
typedef std::map< int, executor::exec_handle > exec_handles_map;


}  // anonymous namespace


/// Basename of the file containing the stdout of the subprocess.
const char* utils::process::executor::detail::stdout_name = "stdout.txt";


/// Basename of the file containing the stderr of the subprocess.
const char* utils::process::executor::detail::stderr_name = "stderr.txt";


/// Basename of the subdirectory in which the subprocess is actually executed.
///
/// This is a subdirectory of the "unique work directory" generated for the
/// subprocess so that our code can create control files on disk and not
/// get them clobbered by the subprocess's activity.
const char* utils::process::executor::detail::work_subdir = "work";


/// Prepares a subprocess to run a user-provided hook in a controlled manner.
///
/// \param unprivileged_user User to switch to if not none.
/// \param control_directory Path to the subprocess-specific control directory.
/// \param work_directory Path to the subprocess-specific work directory.
void
utils::process::executor::detail::setup_child(
    const optional< passwd::user > unprivileged_user,
    const fs::path& control_directory,
    const fs::path& work_directory)
{
    logging::set_inmemory();
    process::isolate_path(unprivileged_user, control_directory);
    process::isolate_child(unprivileged_user, work_directory);
}


/// Internal implementation for the exec_handle class.
struct utils::process::executor::exec_handle::impl : utils::noncopyable {
    /// PID of the process being run.
    int pid;

    /// Path to the subprocess-specific work directory.
    fs::path control_directory;

    /// Path to the subprocess's stdout file.
    const fs::path stdout_file;

    /// Path to the subprocess's stderr file.
    const fs::path stderr_file;

    /// Start time.
    datetime::timestamp start_time;

    /// User the subprocess is running as if different than the current one.
    const optional< passwd::user > unprivileged_user;

    /// Timer to kill the subprocess on activation.
    process::deadline_killer timer;

    /// Number of owners of the on-disk state.
    executor::detail::refcnt_t state_owners;

    /// Constructor.
    ///
    /// \param pid_ PID of the forked process.
    /// \param control_directory_ Path to the subprocess-specific work
    ///     directory.
    /// \param stdout_file_ Path to the subprocess's stdout file.
    /// \param stderr_file_ Path to the subprocess's stderr file.
    /// \param start_time_ Timestamp of when this object was constructed.
    /// \param timeout Maximum amount of time the subprocess can run for.
    /// \param unprivileged_user_ User the subprocess is running as if
    ///     different than the current one.
    /// \param [in,out] state_owners_ Number of owners of the on-disk state.
    ///     For first-time processes, this should be a new counter set to 0;
    ///     for followup processes, this should point to the same counter used
    ///     by the preceding process.
    impl(const int pid_,
         const fs::path& control_directory_,
         const fs::path& stdout_file_,
         const fs::path& stderr_file_,
         const datetime::timestamp& start_time_,
         const datetime::delta& timeout,
         const optional< passwd::user > unprivileged_user_,
         executor::detail::refcnt_t state_owners_) :
        pid(pid_),
        control_directory(control_directory_),
        stdout_file(stdout_file_),
        stderr_file(stderr_file_),
        start_time(start_time_),
        unprivileged_user(unprivileged_user_),
        timer(timeout, pid_),
        state_owners(state_owners_)
    {
        (*state_owners)++;
        POST(*state_owners > 0);
    }
};


/// Constructor.
///
/// \param pimpl Constructed internal implementation.
executor::exec_handle::exec_handle(std::shared_ptr< impl > pimpl) :
    _pimpl(pimpl)
{
}


/// Destructor.
executor::exec_handle::~exec_handle(void)
{
}


/// Returns the PID of the process being run.
///
/// \return A PID.
int
executor::exec_handle::pid(void) const
{
    return _pimpl->pid;
}


/// Returns the path to the subprocess-specific control directory.
///
/// This is where the executor may store control files.
///
/// \return The path to a directory that exists until cleanup() is called.
fs::path
executor::exec_handle::control_directory(void) const
{
    return _pimpl->control_directory;
}


/// Returns the path to the subprocess-specific work directory.
///
/// This is guaranteed to be clear of files created by the executor.
///
/// \return The path to a directory that exists until cleanup() is called.
fs::path
executor::exec_handle::work_directory(void) const
{
    return _pimpl->control_directory / detail::work_subdir;
}


/// Returns the path to the subprocess's stdout file.
///
/// \return The path to a file that exists until cleanup() is called.
const fs::path&
executor::exec_handle::stdout_file(void) const
{
    return _pimpl->stdout_file;
}


/// Returns the path to the subprocess's stderr file.
///
/// \return The path to a file that exists until cleanup() is called.
const fs::path&
executor::exec_handle::stderr_file(void) const
{
    return _pimpl->stderr_file;
}


/// Internal implementation for the exit_handle class.
struct utils::process::executor::exit_handle::impl : utils::noncopyable {
    /// Original PID of the terminated subprocess.
    ///
    /// Note that this PID is no longer valid and cannot be used on system
    /// tables!
    const int original_pid;

    /// Termination status of the subprocess, or none if it timed out.
    const optional< process::status > status;

    /// The user the process ran as, if different than the current one.
    const optional< passwd::user > unprivileged_user;

    /// Timestamp of when the subprocess was spawned.
    const datetime::timestamp start_time;

    /// Timestamp of when wait() or wait_any() returned this object.
    const datetime::timestamp end_time;

    /// Path to the subprocess-specific work directory.
    const fs::path control_directory;

    /// Path to the subprocess's stdout file.
    const fs::path stdout_file;

    /// Path to the subprocess's stderr file.
    const fs::path stderr_file;

    /// Number of owners of the on-disk state.
    ///
    /// This will be 1 if this exit_handle is the last holder of the on-disk
    /// state, in which case cleanup() invocations will wipe the disk state.
    /// For all other cases, this will hold a higher value.
    detail::refcnt_t state_owners;

    /// Mutable pointer to the corresponding executor state.
    ///
    /// This object references a member of the executor_handle that yielded this
    /// exit_handle instance.  We need this direct access to clean up after
    /// ourselves when the handle is destroyed.
    exec_handles_map& all_exec_handles;

    /// Whether the subprocess state has been cleaned yet or not.
    ///
    /// Used to keep track of explicit calls to the public cleanup().
    bool cleaned;

    /// Constructor.
    ///
    /// \param original_pid_ Original PID of the terminated subprocess.
    /// \param status_ Termination status of the subprocess, or none if
    ///     timed out.
    /// \param unprivileged_user_ The user the process ran as, if different than
    ///     the current one.
    /// \param start_time_ Timestamp of when the subprocess was spawned.
    /// \param end_time_ Timestamp of when wait() or wait_any() returned this
    ///     object.
    /// \param control_directory_ Path to the subprocess-specific work
    ///     directory.
    /// \param stdout_file_ Path to the subprocess's stdout file.
    /// \param stderr_file_ Path to the subprocess's stderr file.
    /// \param [in,out] state_owners_ Number of owners of the on-disk state.
    /// \param [in,out] all_exec_handles_ Global object keeping track of all
    ///     active executions for an executor.  This is a pointer to a member of
    ///     the executor_handle object.
    impl(const int original_pid_,
         const optional< process::status > status_,
         const optional< passwd::user > unprivileged_user_,
         const datetime::timestamp& start_time_,
         const datetime::timestamp& end_time_,
         const fs::path& control_directory_,
         const fs::path& stdout_file_,
         const fs::path& stderr_file_,
         detail::refcnt_t state_owners_,
         exec_handles_map& all_exec_handles_) :
        original_pid(original_pid_), status(status_),
        unprivileged_user(unprivileged_user_),
        start_time(start_time_), end_time(end_time_),
        control_directory(control_directory_),
        stdout_file(stdout_file_), stderr_file(stderr_file_),
        state_owners(state_owners_),
        all_exec_handles(all_exec_handles_), cleaned(false)
    {
    }

    /// Destructor.
    ~impl(void)
    {
        if (!cleaned) {
            LW(F("Implicitly cleaning up exit_handle for exec_handle %s; "
                 "ignoring errors!") % original_pid);
            try {
                cleanup();
            } catch (const std::runtime_error& error) {
                LE(F("Subprocess cleanup failed: %s") % error.what());
            }
        }
    }

    /// Cleans up the subprocess on-disk state.
    ///
    /// \throw engine::error If the cleanup fails, especially due to the
    ///     inability to remove the work directory.
    void
    cleanup(void)
    {
        PRE(*state_owners > 0);
        if (*state_owners == 1) {
            LI(F("Cleaning up exit_handle for exec_handle %s") % original_pid);
            fs::rm_r(control_directory);
        } else {
            LI(F("Not cleaning up exit_handle for exec_handle %s; "
                 "%s owners left") % original_pid % (*state_owners - 1));
        }
        // We must decrease our reference only after we have successfully
        // cleaned up the control directory.  Otherwise, the rm_r call would
        // throw an exception, which would in turn invoke the implicit cleanup
        // from the destructor, which would make us crash due to an invalid
        // reference count.
        (*state_owners)--;
        // Marking this object as clean here, even if we did not do actually the
        // cleaning above, is fine (albeit a bit confusing).  Note that "another
        // owner" refers to a handle for a different PID, so that handle will be
        // the one issuing the cleanup.
        all_exec_handles.erase(original_pid);
        cleaned = true;
    }
};


/// Constructor.
///
/// \param pimpl Constructed internal implementation.
executor::exit_handle::exit_handle(std::shared_ptr< impl > pimpl) :
    _pimpl(pimpl)
{
}


/// Destructor.
executor::exit_handle::~exit_handle(void)
{
}


/// Cleans up the subprocess status.
///
/// This function should be called explicitly as it provides the means to
/// control any exceptions raised during cleanup.  Do not rely on the destructor
/// to clean things up.
///
/// \throw engine::error If the cleanup fails, especially due to the inability
///     to remove the work directory.
void
executor::exit_handle::cleanup(void)
{
    PRE(!_pimpl->cleaned);
    _pimpl->cleanup();
    POST(_pimpl->cleaned);
}


/// Gets the current number of owners of the on-disk data.
///
/// \return A shared reference counter.  Even though this function is marked as
/// const, the return value is intentionally mutable because we need to update
/// reference counts from different but related processes.  This is why this
/// method is not public.
std::shared_ptr< std::size_t >
executor::exit_handle::state_owners(void) const
{
    return _pimpl->state_owners;
}


/// Returns the original PID corresponding to the terminated subprocess.
///
/// \return An exec_handle.
int
executor::exit_handle::original_pid(void) const
{
    return _pimpl->original_pid;
}


/// Returns the process termination status of the subprocess.
///
/// \return A process termination status, or none if the subprocess timed out.
const optional< process::status >&
executor::exit_handle::status(void) const
{
    return _pimpl->status;
}


/// Returns the user the process ran as if different than the current one.
///
/// \return None if the credentials of the process were the same as the current
/// one, or else a user.
const optional< passwd::user >&
executor::exit_handle::unprivileged_user(void) const
{
    return _pimpl->unprivileged_user;
}


/// Returns the timestamp of when the subprocess was spawned.
///
/// \return A timestamp.
const datetime::timestamp&
executor::exit_handle::start_time(void) const
{
    return _pimpl->start_time;
}


/// Returns the timestamp of when wait() or wait_any() returned this object.
///
/// \return A timestamp.
const datetime::timestamp&
executor::exit_handle::end_time(void) const
{
    return _pimpl->end_time;
}


/// Returns the path to the subprocess-specific control directory.
///
/// This is where the executor may store control files.
///
/// \return The path to a directory that exists until cleanup() is called.
fs::path
executor::exit_handle::control_directory(void) const
{
    return _pimpl->control_directory;
}


/// Returns the path to the subprocess-specific work directory.
///
/// This is guaranteed to be clear of files created by the executor.
///
/// \return The path to a directory that exists until cleanup() is called.
fs::path
executor::exit_handle::work_directory(void) const
{
    return _pimpl->control_directory / detail::work_subdir;
}


/// Returns the path to the subprocess's stdout file.
///
/// \return The path to a file that exists until cleanup() is called.
const fs::path&
executor::exit_handle::stdout_file(void) const
{
    return _pimpl->stdout_file;
}


/// Returns the path to the subprocess's stderr file.
///
/// \return The path to a file that exists until cleanup() is called.
const fs::path&
executor::exit_handle::stderr_file(void) const
{
    return _pimpl->stderr_file;
}


/// Internal implementation for the executor_handle.
///
/// Because the executor is a singleton, these essentially is a container for
/// global variables.
struct utils::process::executor::executor_handle::impl : utils::noncopyable {
    /// Numeric counter of executed subprocesses.
    ///
    /// This is used to generate a unique identifier for each subprocess as an
    /// easy mechanism to discern their unique work directories.
    size_t last_subprocess;

    /// Interrupts handler.
    std::unique_ptr< signals::interrupts_handler > interrupts_handler;

    /// Root work directory for all executed subprocesses.
    std::unique_ptr< fs::auto_directory > root_work_directory;

    /// Mapping of PIDs to the data required at run time.
    exec_handles_map all_exec_handles;

    /// Former members of all_exec_handles removed due to PID reuse.
    std::forward_list<exec_handle> stale_exec_handles;

    /// Whether the executor state has been cleaned yet or not.
    ///
    /// Used to keep track of explicit calls to the public cleanup().
    bool cleaned;

    /// Constructor.
    impl(void) :
        last_subprocess(0),
        interrupts_handler(new signals::interrupts_handler()),
        root_work_directory(new fs::auto_directory(
            fs::auto_directory::mkdtemp_public(work_directory_template))),
        all_exec_handles(),
        stale_exec_handles(),
        cleaned(false)
    {
    }

    /// Destructor.
    ~impl(void)
    {
        if (!cleaned) {
            LW("Implicitly cleaning up executor; ignoring errors!");
            try {
                cleanup();
                cleaned = true;
            } catch (const std::runtime_error& error) {
                LE(F("Executor global cleanup failed: %s") % error.what());
            }
        }
    }

    /// Cleans up the executor state.
    void
    cleanup(void)
    {
        PRE(!cleaned);

        for (exec_handles_map::const_iterator iter = all_exec_handles.begin();
             iter != all_exec_handles.end(); ++iter) {
            const int& pid = (*iter).first;
            const exec_handle& data = (*iter).second;

            process::terminate_group(pid);
            int status;
            if (::waitpid(pid, &status, 0) == -1) {
                // Should not happen.
                LW(F("Failed to wait for PID %s") % pid);
            }

            try {
                fs::rm_r(data.control_directory());
            } catch (const fs::error& e) {
                LE(F("Failed to clean up subprocess work directory %s: %s") %
                   data.control_directory() % e.what());
            }
        }
        all_exec_handles.clear();

        for (auto iter : stale_exec_handles) {
            // The process already exited, so no need to kill and wait.
            try {
                fs::rm_r(iter.control_directory());
            } catch (const fs::error& e) {
                LE(F("Failed to clean up stale subprocess work directory "
                    "%s: %s") % iter.control_directory() % e.what());
            }
        }
        stale_exec_handles.clear();

        try {
            // The following only causes the work directory to be deleted, not
            // any of its contents, so we expect this to always succeed.  This
            // *should* be sufficient because, in the loop above, we have
            // individually wiped the subdirectories of any still-unclean
            // subprocesses.
            root_work_directory->cleanup();
        } catch (const fs::error& e) {
            LE(F("Failed to clean up executor work directory %s: %s; "
                "this could be an internal error or a buggy test") %
                root_work_directory->directory() % e.what());
        }
        root_work_directory.reset();

        interrupts_handler->unprogram();
        interrupts_handler.reset();
    }

    /// Common code to run after any of the wait calls.
    ///
    /// \param original_pid The PID of the terminated subprocess.
    /// \param status The exit status of the terminated subprocess.
    ///
    /// \return A pointer to an object describing the waited-for subprocess.
    executor::exit_handle
    post_wait(const int original_pid, const process::status& status)
    {
        PRE(original_pid == status.dead_pid());
        LI(F("Waited for subprocess with exec_handle %s") % original_pid);

        process::terminate_group(status.dead_pid());

        const exec_handles_map::iterator iter = all_exec_handles.find(
            original_pid);
        exec_handle& data = (*iter).second;
        data._pimpl->timer.unprogram();

        // It is tempting to assert here (and old code did) that, if the timer
        // has fired, the process has been forcibly killed by us.  This is not
        // always the case though: for short-lived processes and with very short
        // timeouts (think 1ms), it is possible for scheduling decisions to
        // allow the subprocess to finish while at the same time cause the timer
        // to fire.  So we do not assert this any longer and just rely on the
        // timer expiration to check if the process timed out or not.  If the
        // process did finish but the timer expired... oh well, we do not detect
        // this correctly but we don't care because this should not really
        // happen.

        if (!fs::exists(data.stdout_file())) {
            std::ofstream new_stdout(data.stdout_file().c_str());
        }
        if (!fs::exists(data.stderr_file())) {
            std::ofstream new_stderr(data.stderr_file().c_str());
        }

        return exit_handle(std::shared_ptr< exit_handle::impl >(
            new exit_handle::impl(
                data.pid(),
                data._pimpl->timer.fired() ?
                    none : utils::make_optional(status),
                data._pimpl->unprivileged_user,
                data._pimpl->start_time, datetime::timestamp::now(),
                data.control_directory(),
                data.stdout_file(),
                data.stderr_file(),
                data._pimpl->state_owners,
                all_exec_handles)));
    }

    executor::exit_handle
    reap(const pid_t original_pid)
    {
        const exec_handles_map::iterator iter = all_exec_handles.find(
            original_pid);
        exec_handle& data = (*iter).second;
        data._pimpl->timer.unprogram();

        if (!fs::exists(data.stdout_file())) {
            std::ofstream new_stdout(data.stdout_file().c_str());
        }
        if (!fs::exists(data.stderr_file())) {
            std::ofstream new_stderr(data.stderr_file().c_str());
        }

        return exit_handle(std::shared_ptr< exit_handle::impl >(
            new exit_handle::impl(
                data.pid(),
                none,
                data._pimpl->unprivileged_user,
                data._pimpl->start_time, datetime::timestamp::now(),
                data.control_directory(),
                data.stdout_file(),
                data.stderr_file(),
                data._pimpl->state_owners,
                all_exec_handles)));
    }
};


/// Constructor.
executor::executor_handle::executor_handle(void) throw() : _pimpl(new impl())
{
}


/// Destructor.
executor::executor_handle::~executor_handle(void)
{
}


/// Queries the path to the root of the work directory for all subprocesses.
///
/// \return A path.
const fs::path&
executor::executor_handle::root_work_directory(void) const
{
    return _pimpl->root_work_directory->directory();
}


/// Cleans up the executor state.
///
/// This function should be called explicitly as it provides the means to
/// control any exceptions raised during cleanup.  Do not rely on the destructor
/// to clean things up.
///
/// \throw engine::error If there are problems cleaning up the executor.
void
executor::executor_handle::cleanup(void)
{
    PRE(!_pimpl->cleaned);
    _pimpl->cleanup();
    _pimpl->cleaned = true;
}


/// Initializes the executor.
///
/// \pre This function can only be called if there is no other executor_handle
/// object alive.
///
/// \return A handle to the operations of the executor.
executor::executor_handle
executor::setup(void)
{
    return executor_handle();
}


/// Pre-helper for the spawn() method.
///
/// \return The created control directory for the subprocess.
fs::path
executor::executor_handle::spawn_pre(void)
{
    signals::check_interrupt();

    ++_pimpl->last_subprocess;

    const fs::path control_directory =
        _pimpl->root_work_directory->directory() /
        (F("%s") % _pimpl->last_subprocess);
    fs::mkdir_p(control_directory / detail::work_subdir, 0755);

    return control_directory;
}


/// Post-helper for the spawn() method.
///
/// \param control_directory Control directory as returned by spawn_pre().
/// \param stdout_file Path to the subprocess' stdout.
/// \param stderr_file Path to the subprocess' stderr.
/// \param timeout Maximum amount of time the subprocess can run for.
/// \param unprivileged_user If not none, user to switch to before execution.
/// \param child The process created by spawn().
///
/// \return The execution handle of the started subprocess.
executor::exec_handle
executor::executor_handle::spawn_post(
    const fs::path& control_directory,
    const fs::path& stdout_file,
    const fs::path& stderr_file,
    const datetime::delta& timeout,
    const optional< passwd::user > unprivileged_user,
    std::unique_ptr< process::child > child)
{
    const exec_handle handle(std::shared_ptr< exec_handle::impl >(
        new exec_handle::impl(
            child->pid(),
            control_directory,
            stdout_file,
            stderr_file,
            datetime::timestamp::now(),
            timeout,
            unprivileged_user,
            detail::refcnt_t(new detail::refcnt_t::element_type(0)))));
    const auto value = exec_handles_map::value_type(handle.pid(), handle);
    auto insert_pair = _pimpl->all_exec_handles.insert(value);
    if (!insert_pair.second) {
        LI(F("PID %s already in all_exec_handles") % handle.pid());
        _pimpl->stale_exec_handles.push_front(insert_pair.first->second);
        _pimpl->all_exec_handles.erase(insert_pair.first);
        insert_pair = _pimpl->all_exec_handles.insert(value);
        INV_MSG(insert_pair.second, F("PID %s still in all_exec_handles") %
            handle.pid());
    }
    LI(F("Spawned subprocess with exec_handle %s") % handle.pid());
    return handle;
}


/// Pre-helper for the spawn_followup() method.
void
executor::executor_handle::spawn_followup_pre(void)
{
    signals::check_interrupt();
}


/// Post-helper for the spawn_followup() method.
///
/// \param base Exit handle of the subprocess to use as context.
/// \param timeout Maximum amount of time the subprocess can run for.
/// \param child The process created by spawn_followup().
///
/// \return The execution handle of the started subprocess.
executor::exec_handle
executor::executor_handle::spawn_followup_post(
    const exit_handle& base,
    const datetime::delta& timeout,
    std::unique_ptr< process::child > child)
{
    INV(*base.state_owners() > 0);
    const exec_handle handle(std::shared_ptr< exec_handle::impl >(
        new exec_handle::impl(
            child->pid(),
            base.control_directory(),
            base.stdout_file(),
            base.stderr_file(),
            datetime::timestamp::now(),
            timeout,
            base.unprivileged_user(),
            base.state_owners())));
    const auto value = exec_handles_map::value_type(handle.pid(), handle);
    auto insert_pair = _pimpl->all_exec_handles.insert(value);
    if (!insert_pair.second) {
        LI(F("PID %s already in all_exec_handles") % handle.pid());
        _pimpl->stale_exec_handles.push_front(insert_pair.first->second);
        _pimpl->all_exec_handles.erase(insert_pair.first);
        insert_pair = _pimpl->all_exec_handles.insert(value);
        INV_MSG(insert_pair.second, F("PID %s still in all_exec_handles") %
            handle.pid());
    }
    LI(F("Spawned subprocess with exec_handle %s") % handle.pid());
    return handle;
}


/// Waits for completion of any forked process.
///
/// \param exec_handle The handle of the process to wait for.
///
/// \return A pointer to an object describing the waited-for subprocess.
executor::exit_handle
executor::executor_handle::wait(const exec_handle exec_handle)
{
    signals::check_interrupt();
    const process::status status = process::wait(exec_handle.pid());
    return _pimpl->post_wait(exec_handle.pid(), status);
}


/// Waits for completion of any forked process.
///
/// \return A pointer to an object describing the waited-for subprocess.
executor::exit_handle
executor::executor_handle::wait_any(void)
{
    signals::check_interrupt();
    const process::status status = process::wait_any();
    return _pimpl->post_wait(status.dead_pid(), status);
}


/// Forms exit_handle for the given PID subprocess.
///
/// Can be used in the cases when we want to do cleanup(s) of a killed test
/// subprocess, but we do not have exit handle as we usually do after normal
/// wait mechanism.
///
/// \return A pointer to an object describing the subprocess.
executor::exit_handle
executor::executor_handle::reap(const int pid)
{
    return _pimpl->reap(pid);
}


/// Checks if an interrupt has fired.
///
/// Calls to this function should be sprinkled in strategic places through the
/// code protected by an interrupts_handler object.
///
/// This is just a wrapper over signals::check_interrupt() to avoid leaking this
/// dependency to the caller.
///
/// \throw signals::interrupted_error If there has been an interrupt.
void
executor::executor_handle::check_interrupt(void) const
{
    signals::check_interrupt();
}

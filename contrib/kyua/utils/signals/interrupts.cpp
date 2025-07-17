// Copyright 2012 The Kyua Authors.
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

#include "utils/signals/interrupts.hpp"

extern "C" {
#include <sys/types.h>

#include <signal.h>
#include <unistd.h>
}

#include <cstdlib>
#include <cstring>
#include <set>

#include "utils/logging/macros.hpp"
#include "utils/process/operations.hpp"
#include "utils/sanity.hpp"
#include "utils/signals/exceptions.hpp"
#include "utils/signals/programmer.hpp"

namespace signals = utils::signals;
namespace process = utils::process;


namespace {


/// The interrupt signal that fired, or -1 if none.
static volatile int fired_signal = -1;


/// Collection of PIDs.
typedef std::set< pid_t > pids_set;


/// List of processes to kill upon reception of a signal.
static pids_set pids_to_kill;


/// Programmer status for the SIGHUP signal.
static std::unique_ptr< signals::programmer > sighup_handler;
/// Programmer status for the SIGINT signal.
static std::unique_ptr< signals::programmer > sigint_handler;
/// Programmer status for the SIGTERM signal.
static std::unique_ptr< signals::programmer > sigterm_handler;


/// Signal mask to restore after exiting a signal inhibited section.
static sigset_t global_old_sigmask;


/// Whether there is an interrupts_handler object in existence or not.
static bool interrupts_handler_active = false;


/// Whether there is an interrupts_inhibiter object in existence or not.
static std::size_t interrupts_inhibiter_active = 0;


/// Generic handler to capture interrupt signals.
///
/// From this handler, we record that an interrupt has happened so that
/// check_interrupt() can know whether there execution has to be stopped or not.
/// We also terminate any of our child processes (started by the
/// utils::process::children class) so that any ongoing wait(2) system calls
/// terminate.
///
/// \param signo The signal that caused this handler to be called.
static void
signal_handler(const int signo)
{
    static const char* message = "[-- Signal caught; please wait for "
        "cleanup --]\n";
    if (::write(STDERR_FILENO, message, std::strlen(message)) == -1) {
        // We are exiting: the message printed here is only for informational
        // purposes.  If we fail to print it (which probably means something
        // is really bad), there is not much we can do within the signal
        // handler, so just ignore this.
    }

    fired_signal = signo;

    for (pids_set::const_iterator iter = pids_to_kill.begin();
        iter != pids_to_kill.end(); ++iter) {
        process::terminate_group(*iter);
    }
}


/// Installs signal handlers for potential interrupts.
///
/// \pre Must not have been called before.
/// \post The various sig*_handler global variables are atomically updated.
static void
setup_handlers(void)
{
    PRE(sighup_handler.get() == NULL);
    PRE(sigint_handler.get() == NULL);
    PRE(sigterm_handler.get() == NULL);

    // Create the handlers on the stack first so that, if any of them fails, the
    // stack unwinding cleans things up.
    std::unique_ptr< signals::programmer > tmp_sighup_handler(
        new signals::programmer(SIGHUP, signal_handler));
    std::unique_ptr< signals::programmer > tmp_sigint_handler(
        new signals::programmer(SIGINT, signal_handler));
    std::unique_ptr< signals::programmer > tmp_sigterm_handler(
        new signals::programmer(SIGTERM, signal_handler));

    // Now, update the global pointers, which is an operation that cannot fail.
    sighup_handler = std::move(tmp_sighup_handler);
    sigint_handler = std::move(tmp_sigint_handler);
    sigterm_handler = std::move(tmp_sigterm_handler);
}


/// Uninstalls the signal handlers installed by setup_handlers().
static void
cleanup_handlers(void)
{
    sighup_handler->unprogram(); sighup_handler.reset();
    sigint_handler->unprogram(); sigint_handler.reset();
    sigterm_handler->unprogram(); sigterm_handler.reset();
}



/// Masks the signals installed by setup_handlers().
///
/// \param[out] old_sigmask The old signal mask to save via the
///     \code oset \endcode argument with sigprocmask(2).
static void
mask_signals(sigset_t* old_sigmask)
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    const int ret = ::sigprocmask(SIG_BLOCK, &mask, old_sigmask);
    INV(ret != -1);
}


/// Resets the signal masking put in place by mask_signals().
///
/// \param[in] old_sigmask The old signal mask to restore via the
///     \code set \endcode argument with sigprocmask(2).
static void
unmask_signals(sigset_t* old_sigmask)
{
    const int ret = ::sigprocmask(SIG_SETMASK, old_sigmask, NULL);
    INV(ret != -1);
}


}  // anonymous namespace


/// Constructor that sets up the signal handlers.
signals::interrupts_handler::interrupts_handler(void) :
    _programmed(false)
{
    PRE(!interrupts_handler_active);
    setup_handlers();
    _programmed = true;
    interrupts_handler_active = true;
}


/// Destructor that removes the signal handlers.
///
/// Given that this is a destructor and it can't report errors back to the
/// caller, the caller must attempt to call unprogram() on its own.
signals::interrupts_handler::~interrupts_handler(void)
{
    if (_programmed) {
        LW("Destroying still-programmed signals::interrupts_handler object");
        try {
            unprogram();
        } catch (const error& e) {
            UNREACHABLE;
        }
    }
}


/// Unprograms all signals captured by the interrupts handler.
///
/// \throw system_error If the unprogramming of any signal fails.
void
signals::interrupts_handler::unprogram(void)
{
    PRE(_programmed);

    // Modify the control variables first before unprogramming the handlers.  If
    // we fail to do the latter, we do not want to try again because we will not
    // succeed (and we'll cause a crash due to failed preconditions).
    _programmed = false;
    interrupts_handler_active = false;

    cleanup_handlers();
    fired_signal = -1;
}


/// Constructor that sets up signal masking.
signals::interrupts_inhibiter::interrupts_inhibiter(void)
{
    sigset_t old_sigmask;
    mask_signals(&old_sigmask);
    if (interrupts_inhibiter_active == 0) {
        global_old_sigmask = old_sigmask;
    }
    ++interrupts_inhibiter_active;
}


/// Destructor that removes signal masking.
signals::interrupts_inhibiter::~interrupts_inhibiter(void)
{
    if (interrupts_inhibiter_active > 1) {
        --interrupts_inhibiter_active;
    } else {
        interrupts_inhibiter_active = false;
        unmask_signals(&global_old_sigmask);
    }
}


/// Checks if an interrupt has fired.
///
/// Calls to this function should be sprinkled in strategic places through the
/// code protected by an interrupts_handler object.
///
/// Only one call to this function will raise an exception per signal received.
/// This is to allow executing cleanup actions without reraising interrupt
/// exceptions unless the user has fired another interrupt.
///
/// \throw interrupted_error If there has been an interrupt.
void
signals::check_interrupt(void)
{
    if (fired_signal != -1) {
        const int original_fired_signal = fired_signal;
        fired_signal = -1;
        throw interrupted_error(original_fired_signal);
    }
}


/// Registers a child process to be killed upon reception of an interrupt.
///
/// \pre Must be called with interrupts being inhibited.  The caller must ensure
/// that the call call to fork() and the addition of the PID happen atomically.
///
/// \param pid The PID of the child process.  Must not have been yet regsitered.
void
signals::add_pid_to_kill(const pid_t pid)
{
    PRE(interrupts_inhibiter_active);
    PRE(pids_to_kill.find(pid) == pids_to_kill.end());
    pids_to_kill.insert(pid);
}


/// Unregisters a child process previously registered via add_pid_to_kill().
///
/// \pre Must be called with interrupts being inhibited.  This is not necessary,
/// but pushing this to the caller simplifies our logic and provides consistency
/// with the add_pid_to_kill() call.
///
/// \param pid The PID of the child process.  Must have been registered
///     previously, and the process must have already been awaited for.
void
signals::remove_pid_to_kill(const pid_t pid)
{
    PRE(interrupts_inhibiter_active);
    PRE(pids_to_kill.find(pid) != pids_to_kill.end());
    pids_to_kill.erase(pid);
}

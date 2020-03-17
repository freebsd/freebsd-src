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

#include "utils/sanity.hpp"

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

extern "C" {
#include <signal.h>
#include <unistd.h>
}

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "utils/format/macros.hpp"
#include "utils/logging/macros.hpp"


namespace {


/// List of fatal signals to be intercepted by the sanity code.
///
/// The tests hardcode this list; update them whenever the list gets updated.
static int fatal_signals[] = { SIGABRT, SIGBUS, SIGSEGV, 0 };


/// The path to the log file to report on crashes.  Be aware that this is empty
/// until install_crash_handlers() is called.
static std::string logfile;


/// Prints a message to stderr.
///
/// Note that this runs from a signal handler.  Calling write() is OK.
///
/// \param message The message to print.
static void
err_write(const std::string& message)
{
    if (::write(STDERR_FILENO, message.c_str(), message.length()) == -1) {
        // We are crashing.  If ::write fails, there is not much we could do,
        // specially considering that we are running within a signal handler.
        // Just ignore the error.
    }
}


/// The crash handler for fatal signals.
///
/// The sole purpose of this is to print some informational data before
/// reraising the original signal.
///
/// \param signo The received signal.
static void
crash_handler(const int signo)
{
    PRE(!logfile.empty());

    err_write(F("*** Fatal signal %s received\n") % signo);
    err_write(F("*** Log file is %s\n") % logfile);
    err_write(F("*** Please report this problem to %s detailing what you were "
                "doing before the crash happened; if possible, include the log "
                "file mentioned above\n") % PACKAGE_BUGREPORT);

    /// The handler is installed with SA_RESETHAND, so this is safe to do.  We
    /// really want to call the default handler to generate any possible core
    /// dumps.
    ::kill(::getpid(), signo);
}


/// Installs a handler for a fatal signal representing a crash.
///
/// When the specified signal is captured, the crash_handler() will be called to
/// print some informational details to the user and, later, the signal will be
/// redelivered using the default handler to obtain a core dump.
///
/// \param signo The fatal signal for which to install a handler.
static void
install_one_crash_handler(const int signo)
{
    struct ::sigaction sa;
    sa.sa_handler = crash_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND;

    if (::sigaction(signo, &sa, NULL) == -1) {
        const int original_errno = errno;
        LW(F("Could not install crash handler for signal %s: %s") %
           signo % std::strerror(original_errno));
    } else
        LD(F("Installed crash handler for signal %s") % signo);
}


/// Returns a textual representation of an assertion type.
///
/// The textual representation is user facing.
///
/// \param type The type of the assertion.  If the type is unknown for whatever
///     reason, a special message is returned.  The code cannot abort in such a
///     case because this code is dealing for assertion errors.
///
/// \return A textual description of the assertion type.
static std::string
format_type(const utils::assert_type type)
{
    switch (type) {
    case utils::invariant: return "Invariant check failed";
    case utils::postcondition: return "Postcondition check failed";
    case utils::precondition: return "Precondition check failed";
    case utils::unreachable: return "Unreachable point reached";
    default: return "UNKNOWN ASSERTION TYPE";
    }
}


}  // anonymous namespace


/// Raises an assertion error.
///
/// This function prints information about the assertion failure and terminates
/// execution immediately by calling std::abort().  This ensures a coredump so
/// that the failure can be analyzed later.
///
/// \param type The assertion type; this influences the printed message.
/// \param file The file in which the assertion failed.
/// \param line The line in which the assertion failed.
/// \param message The failure message associated to the condition.
void
utils::sanity_failure(const assert_type type, const char* file,
                      const size_t line, const std::string& message)
{
    std::cerr << "*** " << file << ":" << line << ": " << format_type(type);
    if (!message.empty())
        std::cerr << ": " << message << "\n";
    else
        std::cerr << "\n";
    std::abort();
}


/// Installs persistent handlers for crash signals.
///
/// Should be called at the very beginning of the execution of the program to
/// ensure that a signal handler for fatal crash signals is installed.
///
/// \pre The function has not been called before.
///
/// \param logfile_ The path to the log file to report during a crash.
void
utils::install_crash_handlers(const std::string& logfile_)
{
    static bool installed = false;
    PRE(!installed);
    logfile = logfile_;

    for (const int* iter = &fatal_signals[0]; *iter != 0; iter++)
        install_one_crash_handler(*iter);

    installed = true;
}

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

#include "utils/signals/programmer.hpp"

extern "C" {
#include <signal.h>
}

#include <cerrno>

#include "utils/format/macros.hpp"
#include "utils/logging/macros.hpp"
#include "utils/noncopyable.hpp"
#include "utils/sanity.hpp"
#include "utils/signals/exceptions.hpp"


namespace utils {
namespace signals {


/// Internal implementation for the signals::programmer class.
struct programmer::impl : utils::noncopyable {
    /// The number of the signal managed by this programmer.
    int signo;

    /// Whether the signal is currently programmed by us or not.
    bool programmed;

    /// The signal handler that we replaced; to be restored on unprogramming.
    struct ::sigaction old_sa;

    /// Initializes the internal implementation of the programmer.
    ///
    /// \param signo_ The signal number.
    impl(const int signo_) :
        signo(signo_),
        programmed(false)
    {
    }
};


} // namespace signals
} // namespace utils


namespace signals = utils::signals;


/// Programs a signal handler.
///
/// \param signo The signal for which to install the handler.
/// \param handler The handler to install.
///
/// \throw signals::system_error If there is an error programming the signal.
signals::programmer::programmer(const int signo, const handler_type handler) :
    _pimpl(new impl(signo))
{
    struct ::sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (::sigaction(_pimpl->signo, &sa, &_pimpl->old_sa) == -1) {
        const int original_errno = errno;
        throw system_error(F("Could not install handler for signal %s") %
                           _pimpl->signo, original_errno);
    } else
        _pimpl->programmed = true;
}


/// Destructor; unprograms the signal handler if still programmed.
///
/// Given that this is a destructor and it can't report errors back to the
/// caller, the caller must attempt to call unprogram() on its own.
signals::programmer::~programmer(void)
{
    if (_pimpl->programmed) {
        LW("Destroying still-programmed signals::programmer object");
        try {
            unprogram();
        } catch (const system_error& e) {
            UNREACHABLE;
        }
    }
}


/// Unprograms the signal handler.
///
/// \pre The signal handler is programmed (i.e. this can only be called once).
///
/// \throw system_error If unprogramming the signal failed.  If this happens,
///     the signal is left programmed, this object forgets about the signal and
///     therefore there is no way to restore the original handler.
void
signals::programmer::unprogram(void)
{
    PRE(_pimpl->programmed);

    // If we fail, we don't want the destructor to attempt to unprogram the
    // handler again, as it would result in a crash.
    _pimpl->programmed = false;

    if (::sigaction(_pimpl->signo, &_pimpl->old_sa, NULL) == -1) {
        const int original_errno = errno;
        throw system_error(F("Could not reset handler for signal %s") %
                           _pimpl->signo, original_errno);
    }
}

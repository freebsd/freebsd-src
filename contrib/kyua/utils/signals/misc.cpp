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

#include "utils/signals/misc.hpp"

#if defined(HAVE_CONFIG_H)
#   include "config.h"
#endif

extern "C" {
#include <signal.h>
}

#include <cerrno>
#include <cstddef>

#include "utils/format/macros.hpp"
#include "utils/logging/macros.hpp"
#include "utils/signals/exceptions.hpp"

namespace signals = utils::signals;


/// Number of the last valid signal.
const int utils::signals::last_signo = LAST_SIGNO;


/// Resets a signal handler to its default behavior.
///
/// \param signo The number of the signal handler to reset.
///
/// \throw signals::system_error If there is a problem trying to reset the
///     signal handler to its default behavior.
void
signals::reset(const int signo)
{
    struct ::sigaction sa;
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (::sigaction(signo, &sa, NULL) == -1) {
        const int original_errno = errno;
        throw system_error(F("Failed to reset signal %s") % signo,
                           original_errno);
    }
}


/// Resets all signals to their default handlers.
///
/// \return True if all signals could be reset properly; false otherwise.
bool
signals::reset_all(void)
{
    bool ok = true;

    for (int signo = 1; signo <= signals::last_signo; ++signo) {
        if (signo == SIGKILL || signo == SIGSTOP) {
            // Don't attempt to reset immutable signals.
        } else {
            try {
                signals::reset(signo);
            } catch (const signals::error& e) {
#if defined(SIGTHR)
                if (signo == SIGTHR) {
                    // If FreeBSD's libthr is loaded, it prevents us from
                    // modifying SIGTHR (at least in 11.0-CURRENT as of
                    // 2015-01-28).  Skip failures for this signal if they
                    // happen to avoid this corner case.
                    continue;
                }
#endif
                LW(e.what());
                ok = false;
            }
        }
    }

    return ok;
}

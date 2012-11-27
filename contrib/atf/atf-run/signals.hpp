//
// Automated Testing Framework (atf)
//
// Copyright (c) 2007 The NetBSD Foundation, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
// CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
// IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#if !defined(_ATF_RUN_SIGNALS_HPP_)
#define _ATF_RUN_SIGNALS_HPP_

extern "C" {
#include <signal.h>
}

namespace atf {
namespace atf_run {

extern const int last_signo;
typedef void (*handler)(const int);

class signal_programmer;

// ------------------------------------------------------------------------
// The "signal_holder" class.
// ------------------------------------------------------------------------

//
// A RAII model to hold a signal while the object is alive.
//
class signal_holder {
    const int m_signo;
    signal_programmer* m_sp;

public:
    signal_holder(const int);
    ~signal_holder(void);

    void process(void);
};

// ------------------------------------------------------------------------
// The "signal_programmer" class.
// ------------------------------------------------------------------------

//
// A RAII model to program a signal while the object is alive.
//
class signal_programmer {
    const int m_signo;
    const handler m_handler;
    bool m_programmed;
    struct sigaction m_oldsa;

public:
    signal_programmer(const int, const handler);
    ~signal_programmer(void);

    void unprogram(void);
};

// ------------------------------------------------------------------------
// Free functions.
// ------------------------------------------------------------------------

void reset(const int);

} // namespace atf_run
} // namespace atf

#endif // !defined(_ATF_RUN_SIGNALS_HPP_)

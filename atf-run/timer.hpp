//
// Automated Testing Framework (atf)
//
// Copyright (c) 2010 The NetBSD Foundation, Inc.
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

#if !defined(_ATF_RUN_ALARM_HPP_)
#define _ATF_RUN_ALARM_HPP_

extern "C" {
#include <sys/types.h>
}

#include <memory>

#include "atf-c++/noncopyable.hpp"

namespace atf {
namespace atf_run {

class signal_programmer;

// ------------------------------------------------------------------------
// The "timer" class.
// ------------------------------------------------------------------------

class timer : noncopyable {
    struct impl;
    std::auto_ptr< impl > m_pimpl;

public:
    timer(const unsigned int);
    virtual ~timer(void);

    bool fired(void) const;
    void set_fired(void);
    virtual void timeout_callback(void) = 0;
};

// ------------------------------------------------------------------------
// The "child_timer" class.
// ------------------------------------------------------------------------

class child_timer : public timer {
    const pid_t m_pid;
    volatile bool& m_terminate;

public:
    child_timer(const unsigned int, const pid_t, volatile bool&);
    virtual ~child_timer(void);

    void timeout_callback(void);
};

} // namespace atf_run
} // namespace atf

#endif // !defined(_ATF_RUN_ALARM_HPP_)

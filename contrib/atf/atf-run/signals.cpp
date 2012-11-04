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

#if defined(HAVE_CONFIG_H)
#include "bconfig.h"
#endif

extern "C" {
#include <signal.h>
#include <unistd.h>
}

#include <cerrno>

#include "atf-c++/detail/exceptions.hpp"
#include "atf-c++/detail/sanity.hpp"

#include "signals.hpp"

namespace impl = atf::atf_run;
#define IMPL_NAME "atf::atf_run"

const int impl::last_signo = LAST_SIGNO;

// ------------------------------------------------------------------------
// The "signal_holder" class.
// ------------------------------------------------------------------------

namespace {

static bool happened[LAST_SIGNO + 1];

static
void
holder_handler(const int signo)
{
    happened[signo] = true;
}

} // anonymous namespace

impl::signal_holder::signal_holder(const int signo) :
    m_signo(signo),
    m_sp(NULL)
{
    happened[signo] = false;
    m_sp = new signal_programmer(m_signo, holder_handler);
}

impl::signal_holder::~signal_holder(void)
{
    if (m_sp != NULL)
        delete m_sp;

    if (happened[m_signo])
        ::kill(::getpid(), m_signo);
}

void
impl::signal_holder::process(void)
{
    if (happened[m_signo]) {
        delete m_sp;
        m_sp = NULL;
        happened[m_signo] = false;
        ::kill(::getpid(), m_signo);
        m_sp = new signal_programmer(m_signo, holder_handler);
    }
}

// ------------------------------------------------------------------------
// The "signal_programmer" class.
// ------------------------------------------------------------------------

impl::signal_programmer::signal_programmer(const int signo, const handler h) :
    m_signo(signo),
    m_handler(h),
    m_programmed(false)
{
    struct ::sigaction sa;

    sa.sa_handler = m_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (::sigaction(m_signo, &sa, &m_oldsa) == -1)
        throw atf::system_error(IMPL_NAME, "Could not install handler for "
                                "signal", errno);
    m_programmed = true;
}

impl::signal_programmer::~signal_programmer(void)
{
    unprogram();
}

void
impl::signal_programmer::unprogram(void)
{
    if (m_programmed) {
        if (::sigaction(m_signo, &m_oldsa, NULL) == -1)
            UNREACHABLE;
        m_programmed = false;
    }
}

// ------------------------------------------------------------------------
// Free functions.
// ------------------------------------------------------------------------

void
impl::reset(const int signo)
{
    struct ::sigaction sa;

    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    (void)::sigaction(signo, &sa, NULL);
}

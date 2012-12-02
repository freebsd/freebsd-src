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

#if defined(HAVE_CONFIG_H)
#   include <bconfig.h>
#endif

extern "C" {
#include <sys/time.h>
}

#include <cerrno>
#include <csignal>
#include <ctime>

extern "C" {
#include "atf-c/defs.h"
}

#include "atf-c++/detail/exceptions.hpp"
#include "atf-c++/detail/sanity.hpp"

#include "signals.hpp"
#include "timer.hpp"

namespace impl = atf::atf_run;
#define IMPL_NAME "atf::atf_run"

#if !defined(HAVE_TIMER_T)
static impl::timer* compat_handle;
#endif

// ------------------------------------------------------------------------
// Auxiliary functions.
// ------------------------------------------------------------------------

#if defined(HAVE_TIMER_T)
static
void
handler(const int signo ATF_DEFS_ATTRIBUTE_UNUSED, siginfo_t* si,
        void* uc ATF_DEFS_ATTRIBUTE_UNUSED)
{
    impl::timer* timer = static_cast< impl::timer* >(si->si_value.sival_ptr);
    timer->set_fired();
    timer->timeout_callback();
}
#else
static
void
handler(const int signo ATF_DEFS_ATTRIBUTE_UNUSED,
        siginfo_t* si ATF_DEFS_ATTRIBUTE_UNUSED,
        void* uc ATF_DEFS_ATTRIBUTE_UNUSED)
{
    compat_handle->set_fired();
    compat_handle->timeout_callback();
}
#endif

// ------------------------------------------------------------------------
// The "timer" class.
// ------------------------------------------------------------------------

struct impl::timer::impl {
#if defined(HAVE_TIMER_T)
    ::timer_t m_timer;
    ::itimerspec m_old_it;
#else
    ::itimerval m_old_it;
#endif

    struct ::sigaction m_old_sa;
    volatile bool m_fired;

    impl(void) : m_fired(false)
    {
    }
};

impl::timer::timer(const unsigned int seconds) :
    m_pimpl(new impl())
{
    struct ::sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = ::handler;
    if (::sigaction(SIGALRM, &sa, &m_pimpl->m_old_sa) == -1)
        throw system_error(IMPL_NAME "::timer::timer",
                           "Failed to set signal handler", errno);

#if defined(HAVE_TIMER_T)
    struct ::sigevent se;
    se.sigev_notify = SIGEV_SIGNAL;
    se.sigev_signo = SIGALRM;
    se.sigev_value.sival_ptr = static_cast< void* >(this);
    se.sigev_notify_function = NULL;
    se.sigev_notify_attributes = NULL;
    if (::timer_create(CLOCK_MONOTONIC, &se, &m_pimpl->m_timer) == -1) {
        ::sigaction(SIGALRM, &m_pimpl->m_old_sa, NULL);
        throw system_error(IMPL_NAME "::timer::timer",
                           "Failed to create timer", errno);
    }

    struct ::itimerspec it;
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_nsec = 0;
    it.it_value.tv_sec = seconds;
    it.it_value.tv_nsec = 0;
    if (::timer_settime(m_pimpl->m_timer, 0, &it, &m_pimpl->m_old_it) == -1) {
       ::sigaction(SIGALRM, &m_pimpl->m_old_sa, NULL);
       ::timer_delete(m_pimpl->m_timer);
        throw system_error(IMPL_NAME "::timer::timer",
                           "Failed to program timer", errno);
    }
#else
    ::itimerval it;
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = 0;
    it.it_value.tv_sec = seconds;
    it.it_value.tv_usec = 0;
    if (::setitimer(ITIMER_REAL, &it, &m_pimpl->m_old_it) == -1) {
        ::sigaction(SIGALRM, &m_pimpl->m_old_sa, NULL);
        throw system_error(IMPL_NAME "::timer::timer",
                           "Failed to program timer", errno);
    }
    INV(compat_handle == NULL);
    compat_handle = this;
#endif
}

impl::timer::~timer(void)
{
#if defined(HAVE_TIMER_T)
    {
        const int ret = ::timer_delete(m_pimpl->m_timer);
        INV(ret != -1);
    }
#else
    {
        const int ret = ::setitimer(ITIMER_REAL, &m_pimpl->m_old_it, NULL);
        INV(ret != -1);
    }
#endif
    const int ret = ::sigaction(SIGALRM, &m_pimpl->m_old_sa, NULL);
    INV(ret != -1);

#if !defined(HAVE_TIMER_T)
    compat_handle = NULL;
#endif
}

bool
impl::timer::fired(void)
    const
{
    return m_pimpl->m_fired;
}

void
impl::timer::set_fired(void)
{
    m_pimpl->m_fired = true;
}

// ------------------------------------------------------------------------
// The "child_timer" class.
// ------------------------------------------------------------------------

impl::child_timer::child_timer(const unsigned int seconds, const pid_t pid,
                               volatile bool& terminate) :
    timer(seconds),
    m_pid(pid),
    m_terminate(terminate)
{
}

impl::child_timer::~child_timer(void)
{
}

void
impl::child_timer::timeout_callback(void)
{
    static const timespec ts = { 1, 0 };
    m_terminate = true;
    ::kill(-m_pid, SIGTERM);
    ::nanosleep(&ts, NULL);
    if (::kill(-m_pid, 0) != -1)
       ::kill(-m_pid, SIGKILL);
}

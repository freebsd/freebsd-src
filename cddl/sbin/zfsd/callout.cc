/*-
 * Copyright (c) 2011 Spectra Logic Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * Authors: Justin T. Gibbs     (Spectra Logic Corporation)
 */

#include <signal.h>
#include <syslog.h>

#include "callout.h"
#include "zfsd.h"
#include "zfsd_exception.h"

std::list<Callout *> Callout::s_activeCallouts;
bool		     Callout::s_alarmFired(false);

void
Callout::Init()
{
	signal(SIGALRM,  Callout::AlarmSignalHandler);
}

inline bool
Callout::Stop()
{
	if (!IsPending())
		return (false);

	for (std::list<Callout *>::iterator it(s_activeCallouts.begin());
	     it != s_activeCallouts.end(); it++) {
		if (*it != this)
			continue;

		it = s_activeCallouts.erase(it);
		if (it != s_activeCallouts.end()) {

			/*
			 * Maintain correct interval for the
			 * callouts that follow the just removed
			 * entry.
			 */
			timeradd(&(*it)->m_interval, &m_interval,
				 &(*it)->m_interval);
		}
		break;
	}
	m_pending = false;
	return (true);
}

bool
Callout::Reset(const timeval &interval, CalloutFunc_t *func, void *arg)
{
	bool cancelled(false);

	if (!timerisset(&interval))
		throw ZfsdException("Callout::Reset: interval of 0");

	cancelled = Stop();

	m_interval = interval;
	m_func     = func;
	m_arg      = arg;
	m_pending  = true;

	std::list<Callout *>::iterator it(s_activeCallouts.begin());
	for (; it != s_activeCallouts.end(); it++) {

		if (timercmp(&(*it)->m_interval, &m_interval, <=)) {
			/*
			 * Decrease our interval by those that come
			 * before us.
			 */
			timersub(&m_interval, &(*it)->m_interval, &m_interval);
		} else {
			/*
			 * Account for the time between the newly
			 * inserted event and those that follow.
			 */
			timersub(&(*it)->m_interval, &m_interval,
				 &(*it)->m_interval);
			break;
		}
	}
	s_activeCallouts.insert(it, this);


	if (s_activeCallouts.front() == this) {
		itimerval timerval = { {0, 0}, m_interval };

		setitimer(ITIMER_REAL, &timerval, NULL);
	}

	return (cancelled);
}

void
Callout::AlarmSignalHandler(int)
{
	s_alarmFired = true;
	ZfsDaemon::WakeEventLoop();
}

void
Callout::ExpireCallouts()
{
	if (!s_alarmFired)
		return;

	s_alarmFired = false;
	if (s_activeCallouts.empty()) {
		/* Callout removal/SIGALRM race was lost. */
		return;
	}

	/*
	 * Expire the first callout (the one we used to set the
	 * interval timer) as well as any callouts following that
	 * expire at the same time (have a zero interval from
	 * the callout before it).
	 */
	do {
		Callout *cur(s_activeCallouts.front());
		s_activeCallouts.pop_front();
		cur->m_pending = false;
		cur->m_func(cur->m_arg);
	} while (!s_activeCallouts.empty()
	      && timerisset(&s_activeCallouts.front()->m_interval) == 0);

	if (!s_activeCallouts.empty()) {
		Callout *next(s_activeCallouts.front());
		itimerval timerval = { { 0, 0 }, next->m_interval };

		setitimer(ITIMER_REAL, &timerval, NULL);
	}
}

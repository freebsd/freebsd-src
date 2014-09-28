/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_FBSD_COMPLETION_H_
#define	_FBSD_COMPLETION_H_

#include <linux/errno.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sleepqueue.h>
#include <sys/kernel.h>
#include <sys/proc.h>

struct completion {
	unsigned int done;
};

#define	INIT_COMPLETION(c)	((c).done = 0)
#define	init_completion(c)	((c)->done = 0)

static inline void
_complete_common(struct completion *c, int all)
{
	int wakeup_swapper;

	sleepq_lock(c);
	c->done++;
	if (all)
		wakeup_swapper = sleepq_broadcast(c, SLEEPQ_SLEEP, 0, 0);
	else
		wakeup_swapper = sleepq_signal(c, SLEEPQ_SLEEP, 0, 0);
	sleepq_release(c);
	if (wakeup_swapper)
		kick_proc0();
}

#define	complete(c)	_complete_common(c, 0)
#define	complete_all(c)	_complete_common(c, 1)

/*
 * Indefinite wait for done != 0 with or without signals.
 */
static inline long
_wait_for_common(struct completion *c, int flags)
{

	flags |= SLEEPQ_SLEEP;
	for (;;) {
		sleepq_lock(c);
		if (c->done)
			break;
		sleepq_add(c, NULL, "completion", flags, 0);
		if (flags & SLEEPQ_INTERRUPTIBLE) {
			if (sleepq_wait_sig(c, 0) != 0)
				return (-ERESTARTSYS);
		} else
			sleepq_wait(c, 0);
	}
	c->done--;
	sleepq_release(c);

	return (0);
}

#define	wait_for_completion(c)	_wait_for_common(c, 0)
#define	wait_for_completion_interuptible(c)				\
	_wait_for_common(c, SLEEPQ_INTERRUPTIBLE)

static inline long
_wait_for_timeout_common(struct completion *c, long timeout, int flags)
{
	long end;

	end = ticks + timeout;
	flags |= SLEEPQ_SLEEP;
	for (;;) {
		sleepq_lock(c);
		if (c->done)
			break;
		sleepq_add(c, NULL, "completion", flags, 0);
		sleepq_set_timeout(c, end - ticks);
		if (flags & SLEEPQ_INTERRUPTIBLE) {
			if (sleepq_timedwait_sig(c, 0) != 0)
				return (-ERESTARTSYS);
		} else
			sleepq_timedwait(c, 0);
	}
	c->done--;
	sleepq_release(c);
	timeout = end - ticks;

	return (timeout > 0 ? timeout : 1);
}

#define	wait_for_completion_timeout(c, timeout)				\
	_wait_for_timeout_common(c, timeout, 0)
#define	wait_for_completion_interruptible_timeout(c, timeout)		\
	_wait_for_timeout_common(c, timeout, SLEEPQ_INTERRUPTIBLE)

static inline int
try_wait_for_completion(struct completion *c)
{
	int isdone;

	isdone = 1;
	sleepq_lock(c);
	if (c->done)
		c->done--;
	else
		isdone = 0;
	sleepq_release(c);
	return (isdone);
}

static inline int
completion_done(struct completion *c)
{
	int isdone;

	isdone = 1;
	sleepq_lock(c);
	if (c->done == 0)
		isdone = 0;
	sleepq_release(c);
	return (isdone);
}

#endif	/* _LINUX_COMPLETION_H_ */

/*
 * Copyright (c) 1996, 1997
 *	HD Associates, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by HD Associates, Inc
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY HD ASSOCIATES AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL HD ASSOCIATES OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/* ksched: Soft real time scheduling based on "rtprio".
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/resource.h>
#include <machine/cpu.h>	/* For need_resched */

#include <posix4/posix4.h>

/* ksched: Real-time extension to support POSIX priority scheduling.
 */

struct ksched {
	struct timespec rr_interval;
};

int ksched_attach(struct ksched **p)
{
	struct ksched *ksched= p31b_malloc(sizeof(*ksched));

	ksched->rr_interval.tv_sec = 0;
	ksched->rr_interval.tv_nsec = 1000000000L / roundrobin_interval();

	*p = ksched;
	return 0;
}

int ksched_detach(struct ksched *p)
{
	p31b_free(p);

	return 0;
}

/*
 * XXX About priorities
 *
 *	POSIX 1003.1b requires that numerically higher priorities be of
 *	higher priority.  It also permits sched_setparam to be
 *	implementation defined for SCHED_OTHER.  I don't like
 *	the notion of inverted priorites for normal processes when
 *  you can use "setpriority" for that.
 *
 *	I'm rejecting sched_setparam for SCHED_OTHER with EINVAL.
 */

/* Macros to convert between the unix (lower numerically is higher priority)
 * and POSIX 1003.1b (higher numerically is higher priority)
 */

#define p4prio_to_rtpprio(P) (RTP_PRIO_MAX - (P))
#define rtpprio_to_p4prio(P) (RTP_PRIO_MAX - (P))

/* These improve readability a bit for me:
 */
#define P1B_PRIO_MIN rtpprio_to_p4prio(RTP_PRIO_MAX)
#define P1B_PRIO_MAX rtpprio_to_p4prio(RTP_PRIO_MIN)

static __inline int
getscheduler(int *ret, struct ksched *ksched, struct proc *p)
{
	int e = 0;

	switch (p->p_rtprio.type)
	{
		case RTP_PRIO_FIFO:
		*ret = SCHED_FIFO;
		break;

		case RTP_PRIO_REALTIME:
		*ret = SCHED_RR;
		break;

		default:
		*ret = SCHED_OTHER;
		break;
	}

	return e;
}

int ksched_setparam(int *ret, struct ksched *ksched,
	struct proc *p, const struct sched_param *param)
{
	int e, policy;

	e = getscheduler(&policy, ksched, p);

	if (e == 0)
	{
		if (policy == SCHED_OTHER)
			e = EINVAL;
		else
			e = ksched_setscheduler(ret, ksched, p, policy, param);
	}

	return e;
}

int ksched_getparam(int *ret, struct ksched *ksched,
	struct proc *p, struct sched_param *param)
{
	if (RTP_PRIO_IS_REALTIME(p->p_rtprio.type))
		param->sched_priority = rtpprio_to_p4prio(p->p_rtprio.prio);

	return 0;
}

/*
 * XXX The priority and scheduler modifications should
 *     be moved into published interfaces in kern/kern_sync.
 *
 * The permissions to modify process p were checked in "p31b_proc()".
 *
 */
int ksched_setscheduler(int *ret, struct ksched *ksched,
	struct proc *p, int policy, const struct sched_param *param)
{
	int e = 0;
	struct rtprio rtp;

	switch(policy)
	{
		case SCHED_RR:
		case SCHED_FIFO:

		if (param->sched_priority >= P1B_PRIO_MIN &&
		param->sched_priority <= P1B_PRIO_MAX)
		{
			rtp.prio = p4prio_to_rtpprio(param->sched_priority);
			rtp.type = (policy == SCHED_FIFO)
				? RTP_PRIO_FIFO : RTP_PRIO_REALTIME;

			p->p_rtprio = rtp;
			need_resched();
		}
		else
			e = EPERM;


		break;

		case SCHED_OTHER:
		{
			rtp.type = RTP_PRIO_NORMAL;
			rtp.prio = p4prio_to_rtpprio(param->sched_priority);
			p->p_rtprio = rtp;

			/* XXX Simply revert to whatever we had for last
			 *     normal scheduler priorities.
			 *     This puts a requirement
			 *     on the scheduling code: You must leave the
			 *     scheduling info alone.
			 */
			need_resched();
		}
		break;
	}

	return e;
}

int ksched_getscheduler(int *ret, struct ksched *ksched, struct proc *p)
{
	return getscheduler(ret, ksched, p);
}

/* ksched_yield: Yield the CPU.
 */
int ksched_yield(int *ret, struct ksched *ksched)
{
	need_resched();
	return 0;
}

int ksched_get_priority_max(int *ret, struct ksched *ksched, int policy)
{
	int e = 0;

	switch (policy)
	{
		case SCHED_FIFO:
		case SCHED_RR:
		*ret = RTP_PRIO_MAX;
		break;

		case SCHED_OTHER:
		*ret =  PRIO_MAX;
		break;

		default:
		e = EINVAL;
	}

	return e;
}

int ksched_get_priority_min(int *ret, struct ksched *ksched, int policy)
{
	int e = 0;

	switch (policy)
	{
		case SCHED_FIFO:
		case SCHED_RR:
		*ret = P1B_PRIO_MIN;
		break;

		case SCHED_OTHER:
		*ret =  PRIO_MIN;
		break;

		default:
		e = EINVAL;
	}

	return e;
}

int ksched_rr_get_interval(int *ret, struct ksched *ksched,
	struct proc *p, struct timespec *timespec)
{
	*timespec = ksched->rr_interval;

	return 0;
}

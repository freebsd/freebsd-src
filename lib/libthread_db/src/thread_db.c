/*
 * Copyright (c) 2004 David Xu <davidxu@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stddef.h>
#include <unistd.h>

#include <proc_service.h>
#include <thread_db.h>
#include "thread_db_int.h"

static TAILQ_HEAD(, td_thragent) proclist = TAILQ_HEAD_INITIALIZER(proclist);

extern struct ta_ops pthread_ops;
#if 0
extern struct ta_ops thr_ops;
extern struct ta_ops c_r_ops;
#endif

static struct ta_ops *ops[] = {
	&pthread_ops,
#if 0
	&thr_ops,
	&c_r_ops
#endif
};

td_err_e
td_init(void)
{
	int i, ret = 0;

	for (i = 0; i < sizeof(ops)/sizeof(ops[0]); ++i) {
		int tmp;
		if ((tmp = ops[i]->to_init()) != 0)
			ret = tmp;
	}
	return (ret);
}

td_err_e
td_ta_new(struct ps_prochandle *ph, td_thragent_t **pta)
{
	int i;

	for (i = 0; i < sizeof(ops)/sizeof(ops[0]); ++i) {
		if (ops[i]->to_ta_new(ph, pta) == 0) {
			(*pta)->ta_ops = ops[i];
			TAILQ_INSERT_HEAD(&proclist, *pta, ta_next);
			return (0);
		}
	}
	return (TD_NOLIBTHREAD);

}

td_err_e
td_ta_delete(td_thragent_t *ta)
{
	TAILQ_REMOVE(&proclist, ta, ta_next);
	return ta->ta_ops->to_ta_delete(ta);
}

td_err_e
td_ta_get_nthreads(const td_thragent_t *ta, int *np)
{
	return ta->ta_ops->to_ta_get_nthreads(ta, np);
}

td_err_e
td_ta_get_ph(const td_thragent_t *ta, struct ps_prochandle **ph)
{
	return ta->ta_ops->to_ta_get_ph(ta, ph);
}

td_err_e
td_ta_map_id2thr(const td_thragent_t *ta, thread_t id, td_thrhandle_t *th)
{
	return ta->ta_ops->to_ta_map_id2thr(ta, id, th);
}

td_err_e
td_ta_map_lwp2thr(const td_thragent_t *ta, lwpid_t lwpid, td_thrhandle_t *th)
{
	return ta->ta_ops->to_ta_map_lwp2thr(ta, lwpid, th);
}

td_err_e
td_ta_thr_iter(const td_thragent_t *ta,
               td_thr_iter_f *callback, void *cbdata_p,
               td_thr_state_e state, int ti_pri,
               sigset_t *ti_sigmask_p,
               unsigned int ti_user_flags)
{
	return ta->ta_ops->to_ta_thr_iter(ta, callback, cbdata_p, state,
			ti_pri, ti_sigmask_p, ti_user_flags);
}

td_err_e
td_ta_tsd_iter(const td_thragent_t *ta, td_key_iter_f *ki, void *arg)
{
	return ta->ta_ops->to_ta_tsd_iter(ta, ki, arg);
}

td_err_e
td_ta_event_addr(const td_thragent_t *ta, td_event_e event, td_notify_t *ptr)
{
	return ta->ta_ops->to_ta_event_addr(ta, event, ptr);
}

td_err_e
td_ta_set_event(const td_thragent_t *ta, td_thr_events_t *events)
{
	return ta->ta_ops->to_ta_set_event(ta, events);
}

td_err_e
td_ta_clear_event(const td_thragent_t *ta, td_thr_events_t *events)
{
	return ta->ta_ops->to_ta_clear_event(ta, events);
}

td_err_e
td_ta_event_getmsg(const td_thragent_t *ta, td_event_msg_t *msg)
{
	return ta->ta_ops->to_ta_event_getmsg(ta, msg);
}

td_err_e
td_ta_setconcurrency(const td_thragent_t *ta, int level)
{
	return ta->ta_ops->to_ta_setconcurrency(ta, level);
}

td_err_e
td_ta_enable_stats(const td_thragent_t *ta, int enable)
{
	return ta->ta_ops->to_ta_enable_stats(ta, enable);
}

td_err_e
td_ta_reset_stats(const td_thragent_t *ta)
{
	return ta->ta_ops->to_ta_reset_stats(ta);
}

td_err_e
td_ta_get_stats(const td_thragent_t *ta, td_ta_stats_t *statsp)
{
	return ta->ta_ops->to_ta_get_stats(ta, statsp);
}

td_err_e
td_thr_validate(const td_thrhandle_t *th)
{
	td_thragent_t *ta = th->th_ta_p;

	return ta->ta_ops->to_thr_validate(th);
}

td_err_e
td_thr_get_info(const td_thrhandle_t *th, td_thrinfo_t *info)
{
	td_thragent_t *ta = th->th_ta_p;

	return ta->ta_ops->to_thr_get_info(th, info);
}

td_err_e
td_thr_getfpregs(const td_thrhandle_t *th, prfpregset_t *fpregset)
{
	td_thragent_t *ta = th->th_ta_p;

	return ta->ta_ops->to_thr_getfpregs(th, fpregset);
}

td_err_e
td_thr_getgregs(const td_thrhandle_t *th, prgregset_t gregs)
{
	td_thragent_t *ta = th->th_ta_p;

	return ta->ta_ops->to_thr_getgregs(th, gregs);
}

td_err_e
td_thr_getxregs(const td_thrhandle_t *th, void *xregs)
{
	td_thragent_t *ta = th->th_ta_p;

	return ta->ta_ops->to_thr_getxregs(th, xregs);
}

td_err_e
td_thr_getxregsize(const td_thrhandle_t *th, int *sizep)
{
	td_thragent_t *ta = th->th_ta_p;

	return ta->ta_ops->to_thr_getxregsize(th, sizep);
}

td_err_e
td_thr_setfpregs(const td_thrhandle_t *th, const prfpregset_t *fpregs)
{
	td_thragent_t *ta = th->th_ta_p;

	return ta->ta_ops->to_thr_setfpregs(th, fpregs);
}

td_err_e
td_thr_setgregs(const td_thrhandle_t *th, prgregset_t gregs)
{
	td_thragent_t *ta = th->th_ta_p;

	return ta->ta_ops->to_thr_setgregs(th, gregs);
}

td_err_e
td_thr_setxregs(const td_thrhandle_t *th, const void *addr)
{
	td_thragent_t *ta = th->th_ta_p;

	return ta->ta_ops->to_thr_setxregs(th, addr);
}

td_err_e
td_thr_event_enable(const td_thrhandle_t *th, int en)
{
	td_thragent_t *ta = th->th_ta_p;
	
	return ta->ta_ops->to_thr_event_enable(th, en);
}

td_err_e
td_thr_set_event(const td_thrhandle_t *th, td_thr_events_t *setp)
{
	td_thragent_t *ta = th->th_ta_p;

	return ta->ta_ops->to_thr_set_event(th, setp);
}

td_err_e
td_thr_clear_event(const td_thrhandle_t *th, td_thr_events_t *setp)
{
	td_thragent_t *ta = th->th_ta_p;

	return ta->ta_ops->to_thr_clear_event(th, setp);
}

td_err_e
td_thr_event_getmsg(const td_thrhandle_t *th, td_event_msg_t *msg)
{
	td_thragent_t *ta = th->th_ta_p;

	return ta->ta_ops->to_thr_event_getmsg(th, msg);
}

td_err_e
td_thr_setprio(const td_thrhandle_t *th, int pri)
{
	td_thragent_t *ta = th->th_ta_p;

	return ta->ta_ops->to_thr_setprio(th, pri);
}

td_err_e
td_thr_setsigpending(const td_thrhandle_t *th, unsigned char n,
	const sigset_t *set)
{
	td_thragent_t *ta = th->th_ta_p;

	return ta->ta_ops->to_thr_setsigpending(th, n, set);
}

td_err_e
td_thr_sigsetmask(const td_thrhandle_t *th, const sigset_t *set)
{
	td_thragent_t *ta = th->th_ta_p;

	return ta->ta_ops->to_thr_sigsetmask(th, set);
}

td_err_e
td_thr_tsd(const td_thrhandle_t *th, const thread_key_t key, void **data)
{
	td_thragent_t *ta = th->th_ta_p;

	return ta->ta_ops->to_thr_tsd(th, key, data);
}

td_err_e
td_thr_dbsuspend(const td_thrhandle_t *th)
{
	td_thragent_t *ta = th->th_ta_p;

	return ta->ta_ops->to_thr_dbsuspend(th);
}

td_err_e
td_thr_dbresume(const td_thrhandle_t *th)
{
	td_thragent_t *ta = th->th_ta_p;

	return ta->ta_ops->to_thr_dbresume(th);
}

td_err_e
td_get_ta(int pid, td_thragent_t **ta_p)
{
	td_thragent_t *ta;
	struct ps_prochandle *ph;

	TAILQ_FOREACH(ta, &proclist, ta_next) {
		td_ta_get_ph(ta, &ph);
		if (ps_getpid(ph) == pid) {
			*ta_p = ta;
			return (TD_OK);
		}
	}
	return (TD_ERR);
}

td_err_e
td_ta_activated(td_thragent_t *ta, int *a)
{
	return ta->ta_ops->to_ta_activated(ta, a);
}

td_err_e
td_thr_sstep(td_thrhandle_t *th, int step)
{
	td_thragent_t *ta = th->th_ta_p;

	return ta->ta_ops->to_thr_sstep(th, step);
}

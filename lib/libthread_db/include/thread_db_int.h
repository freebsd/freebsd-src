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
 *
 * $FreeBSD$
 */

#ifndef _THREAD_DB_INT_H
#define _THREAD_DB_INT_H

#include <sys/types.h>
#include <sys/queue.h>

struct td_thragent {
	struct ta_ops			*ta_ops;
	TAILQ_ENTRY(td_thragent)	ta_next;
};

struct ta_ops {
	td_err_e (*to_init)(void);
	td_err_e (*to_ta_new)(struct ps_prochandle *, td_thragent_t **);
	td_err_e (*to_ta_delete)(td_thragent_t *);
	td_err_e (*to_ta_get_nthreads)(const td_thragent_t *, int *);
	td_err_e (*to_ta_get_ph)(const td_thragent_t *, struct ps_prochandle **);
	td_err_e (*to_ta_map_id2thr)(const td_thragent_t *, thread_t, td_thrhandle_t *);
	td_err_e (*to_ta_map_lwp2thr)(const td_thragent_t *, lwpid_t lwpid,
			td_thrhandle_t *);
	td_err_e (*to_ta_thr_iter)(const td_thragent_t *, td_thr_iter_f *, void *,
			td_thr_state_e, int, sigset_t *, unsigned int);
	td_err_e (*to_ta_tsd_iter)(const td_thragent_t *, td_key_iter_f *, void *);
	td_err_e (*to_ta_event_addr)(const td_thragent_t *, td_event_e , td_notify_t *);
	td_err_e (*to_ta_set_event)(const td_thragent_t *, td_thr_events_t *);
	td_err_e (*to_ta_clear_event)(const td_thragent_t *, td_thr_events_t *);
	td_err_e (*to_ta_event_getmsg)(const td_thragent_t *, td_event_msg_t *);
	td_err_e (*to_ta_setconcurrency)(const td_thragent_t *, int);
	td_err_e (*to_ta_enable_stats)(const td_thragent_t *, int);
	td_err_e (*to_ta_reset_stats)(const td_thragent_t *);
	td_err_e (*to_ta_get_stats)(const td_thragent_t *, td_ta_stats_t *);
	td_err_e (*to_thr_validate)(const td_thrhandle_t *);
	td_err_e (*to_thr_get_info)(const td_thrhandle_t *, td_thrinfo_t *);
	td_err_e (*to_thr_getfpregs)(const td_thrhandle_t *, prfpregset_t *);
	td_err_e (*to_thr_getgregs)(const td_thrhandle_t *, prgregset_t);
	td_err_e (*to_thr_getxregs)(const td_thrhandle_t *, void *);
	td_err_e (*to_thr_getxregsize)(const td_thrhandle_t *, int *);
	td_err_e (*to_thr_setfpregs)(const td_thrhandle_t *, const prfpregset_t *);
	td_err_e (*to_thr_setgregs)(const td_thrhandle_t *, const prgregset_t);
	td_err_e (*to_thr_setxregs)(const td_thrhandle_t *, const void *);
	td_err_e (*to_thr_event_enable)(const td_thrhandle_t *, int);
	td_err_e (*to_thr_set_event)(const td_thrhandle_t *, td_thr_events_t *);
	td_err_e (*to_thr_clear_event)(const td_thrhandle_t *, td_thr_events_t *);
	td_err_e (*to_thr_event_getmsg)(const td_thrhandle_t *, td_event_msg_t *);
	td_err_e (*to_thr_setprio)(const td_thrhandle_t *, int);
	td_err_e (*to_thr_setsigpending)(const td_thrhandle_t *, unsigned char,
		const sigset_t *);
	td_err_e (*to_thr_sigsetmask)(const td_thrhandle_t *, const sigset_t *);
	td_err_e (*to_thr_tsd)(const td_thrhandle_t *, const thread_key_t, void **);
	td_err_e (*to_thr_dbsuspend)(const td_thrhandle_t *);
	td_err_e (*to_thr_dbresume)(const td_thrhandle_t *);
	td_err_e (*to_get_ta)(int pid, td_thragent_t **);
	td_err_e (*to_ta_activated)(td_thragent_t *, int *);
	td_err_e (*to_thr_sstep)(td_thrhandle_t *, int step);
};

#ifdef TD_DEBUG
#define TDBG(...) ps_plog(__VA_ARGS__)
#define TDBG_FUNC() ps_plog(__func__); ps_plog("\n")
#else
#define TDBG(...)
#define TDBG_FUNC()
#endif

#endif

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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
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

#ifndef _THREAD_DB_H_
#define _THREAD_DB_H_

#include <sys/types.h>
#include <pthread.h>

typedef enum
{
	TD_OK,
	TD_ERR,
	TD_NOTHR,
	TD_NOSV,
	TD_NOLWP,
	TD_BADPH,
	TD_BADTH,
	TD_BADSH,
	TD_BADTA,
	TD_BADKEY,
	TD_NOMSG,
	TD_NOFPREGS,
	TD_NOLIBTHREAD,
	TD_NOEVENT,
	TD_NOCAPAB,
	TD_DBERR,
	TD_NOAPLIC,
	TD_NOTSD,
	TD_MALLOC,
	TD_PARTIALREG,
	TD_NOXREGS
} td_err_e;

typedef enum
{
	TD_THR_ANY_STATE,
	TD_THR_UNKNOWN,
	TD_THR_STOPPED,
	TD_THR_RUN,
	TD_THR_ACTIVE,
	TD_THR_ZOMBIE,
	TD_THR_SLEEP,
	TD_THR_STOPPED_ASLEEP
} td_thr_state_e;

typedef enum
{
	TD_THR_ANY_TYPE,
	TD_THR_USER,
	TD_THR_SYSTEM
} td_thr_type_e;

typedef long			thread_t;
typedef pthread_key_t		thread_key_t;
typedef struct td_thragent	td_thragent_t;

typedef struct td_thrhandle
{
	td_thragent_t	*th_ta_p;
	thread_t	th_unique;
	int		th_ta_data;
} td_thrhandle_t;

/* Flags for `td_ta_thr_iter'.  */
#define TD_THR_ANY_USER_FLAGS	0xffffffff
#define TD_THR_LOWEST_PRIORITY	0
#define TD_SIGNO_MASK		NULL

typedef uint32_t td_thr_events_t;

typedef enum
{
	TD_ALL_EVENTS,
	TD_EVENT_NONE = TD_ALL_EVENTS,
	TD_CREATE,
	TD_DEATH,
	TD_REAP,
	TD_READY,
	TD_SLEEP,
	TD_SWITCHTO,
	TD_SWITCHFROM,
	TD_LOCK_TRY,
	TD_CATCHSIG,
	TD_IDLE,
	TD_PREEMPT,
	TD_PRI_INHERIT,
	TD_CONCURRENCY,
	TD_TIMEOUT,
	TD_MIN_EVENT_NUM = TD_READY,
	TD_MAX_EVENT_NUM = TD_TIMEOUT,
	TD_EVENTS_ENABLE = 31
} td_event_e;

typedef enum
{
	NOTIFY_BPT,
	NOTIFY_AUTOBPT,
	NOTIFY_SYSCALL
} td_notify_e;

typedef struct td_notify
{
	td_notify_e type;
	union {
		psaddr_t bptaddr;
		int syscallno;
	} u;
} td_notify_t;

typedef struct td_event_msg
{
	td_event_e event;
	const td_thrhandle_t *th_p;
	union {
#if 0
	    td_synchandle_t *sh;
#endif
	    uintptr_t data;
	} msg;
} td_event_msg_t;

/* Structure containing event data available in each thread structure.  */
typedef struct
{
	td_thr_events_t eventmask;	/* Mask of enabled events.  */
	td_event_e eventnum;		/* Number of last event.  */
	void *eventdata;		/* Data associated with event.  */
} td_eventbuf_t;

/* Gathered statistics about the process.  */
typedef struct td_ta_stats
{
	int nthreads;       	/* Total number of threads in use.  */
	int r_concurrency;	/* Concurrency level requested by user.  */
	int nrunnable_num;	/* Average runnable threads, numerator.  */
	int nrunnable_den;	/* Average runnable threads, denominator.  */
	int a_concurrency_num;	/* Achieved concurrency level, numerator.  */
	int a_concurrency_den;	/* Achieved concurrency level, denominator.  */
	int nlwps_num;		/* Average number of processes in use,
				   numerator.  */
	int nlwps_den;		/* Average number of processes in use,
				   denominator.  */
	int nidle_num;		/* Average number of idling processes,
				   numerator.  */
	int nidle_den;		/* Average number of idling processes,
				   denominator.  */
} td_ta_stats_t;

static inline void
td_event_emptyset(td_thr_events_t *setp)
{
	*setp = 0;
}

static inline void
td_event_fillset(td_thr_events_t *setp)
{
	*setp = 0xFFFFFFFF;
}

static inline void
td_event_addset(td_thr_events_t *setp, int n)
{
	*setp |= (1 << (n-1));
}

static inline void
td_event_delset(td_thr_events_t *setp, int n)
{
	*setp &= ~(1 << (n-1));
}

static inline int
td_eventismember(td_thr_events_t *setp, int n)
{
	return (*setp & (1 << (n-1)) ? 1 : 0);
}

static inline int
td_eventisempty(td_thr_events_t *setp)
{
	return (*setp == 0);
}

typedef int td_thr_iter_f(const td_thrhandle_t *, void *);
typedef int td_key_iter_f(thread_key_t, void (*) (void *), void *);

struct ps_prochandle;

typedef struct td_thrinfo
{
	td_thragent_t	*ti_ta_p;
	unsigned int	ti_user_flags;
	thread_t	ti_tid;
	char		*ti_tls;
	psaddr_t	ti_startfunc;
	psaddr_t	ti_stkbase;
	long int	ti_stksize;
	psaddr_t	ti_ro_area;
	int		ti_ro_size;
	td_thr_state_e	ti_state;
	unsigned char	ti_db_suspended;
	td_thr_type_e	ti_type;
	intptr_t	ti_pc;
	intptr_t	ti_sp;
	short int	ti_flags;
	int		ti_pri;
	lwpid_t		ti_lid;
	sigset_t	ti_sigmask;
	unsigned char	ti_traceme;
	unsigned char	ti_preemptflag;
	unsigned char	ti_pirecflag;
	sigset_t	ti_pending;
	td_thr_events_t ti_events;
} td_thrinfo_t;

td_err_e td_init(void);
td_err_e td_log(void);
td_err_e td_ta_new(struct ps_prochandle *, td_thragent_t **);
td_err_e td_ta_delete(td_thragent_t *);
td_err_e td_ta_get_nthreads(const td_thragent_t *, int *);
td_err_e td_ta_get_ph(const td_thragent_t *, struct ps_prochandle **);
td_err_e td_ta_map_id2thr(const td_thragent_t *, thread_t, td_thrhandle_t *);
td_err_e td_ta_map_lwp2thr(const td_thragent_t *, lwpid_t lwpid,
		td_thrhandle_t *);
td_err_e td_ta_thr_iter(const td_thragent_t *, td_thr_iter_f *, void *,
		td_thr_state_e, int, sigset_t *, unsigned int);
td_err_e td_ta_tsd_iter(const td_thragent_t *, td_key_iter_f *, void *);
td_err_e td_ta_event_addr(const td_thragent_t *, td_event_e , td_notify_t *);
td_err_e td_ta_set_event(const td_thragent_t *, td_thr_events_t *);
td_err_e td_ta_clear_event(const td_thragent_t *, td_thr_events_t *);
td_err_e td_ta_event_getmsg(const td_thragent_t *, td_event_msg_t *);
td_err_e td_ta_setconcurrency(const td_thragent_t *, int);
td_err_e td_ta_enable_stats(const td_thragent_t *, int);
td_err_e td_ta_reset_stats(const td_thragent_t *);
td_err_e td_ta_get_stats(const td_thragent_t *, td_ta_stats_t *);
td_err_e td_thr_validate(const td_thrhandle_t *);
td_err_e td_thr_get_info(const td_thrhandle_t *, td_thrinfo_t *);
td_err_e td_thr_getfpregs(const td_thrhandle_t *, prfpregset_t *);
td_err_e td_thr_getgregs(const td_thrhandle_t *, prgregset_t);
td_err_e td_thr_getxregs(const td_thrhandle_t *, void *);
td_err_e td_thr_getxregsize(const td_thrhandle_t *, int *);
td_err_e td_thr_setfpregs(const td_thrhandle_t *, const prfpregset_t *);
td_err_e td_thr_setgregs(const td_thrhandle_t *, const prgregset_t);
td_err_e td_thr_setxregs(const td_thrhandle_t *, const void *);
td_err_e td_thr_event_enable(const td_thrhandle_t *, int);
td_err_e td_thr_set_event(const td_thrhandle_t *, td_thr_events_t *);
td_err_e td_thr_clear_event(const td_thrhandle_t *, td_thr_events_t *);
td_err_e td_thr_event_getmsg(const td_thrhandle_t *, td_event_msg_t *);
td_err_e td_thr_setprio(const td_thrhandle_t *, int);
td_err_e td_thr_setsigpending(const td_thrhandle_t *, unsigned char,
	const sigset_t *);
td_err_e td_thr_sigsetmask(const td_thrhandle_t *, const sigset_t *);
td_err_e td_thr_tsd(const td_thrhandle_t *, const thread_key_t, void **);
td_err_e td_thr_dbsuspend(const td_thrhandle_t *);
td_err_e td_thr_dbresume(const td_thrhandle_t *);
td_err_e td_get_ta(int pid, td_thragent_t **);
td_err_e td_ta_activated(td_thragent_t *, int *);
td_err_e td_thr_sstep(td_thrhandle_t *, int);

#endif

/*-
 * Copyright (c) 1996, 1997
 *      HD Associates, Inc.  All rights reserved.
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
 *      This product includes software developed by HD Associates, Inc
 *      and Jukka Antero Ukkonen.
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
 */

/*-
 * Copyright (c) 2002, Jeffrey Roberson <jeff@freebsd.org>
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
 *
 * $FreeBSD$
 */

#ifndef _SCHED_H_
#define	_SCHED_H_

#ifdef _KERNEL
/*
 * General scheduling info.
 *
 * sched_load:
 *	Total runnable non-ithread threads in the system.
 *
 * sched_runnable:
 *	Runnable threads for this processor.
 */
int	sched_load(void);
int	sched_rr_interval(void);
int	sched_runnable(void);

/* 
 * Proc related scheduling hooks.
 */
void	sched_exit(struct proc *p, struct thread *childtd);
void	sched_fork(struct thread *td, struct thread *childtd);
void	sched_fork_exit(struct thread *td);

/*
 * KSE Groups contain scheduling priority information.  They record the
 * behavior of groups of KSEs and threads.
 */
void	sched_class(struct thread *td, int class);
void	sched_nice(struct proc *p, int nice);

/*
 * Threads are switched in and out, block on resources, have temporary
 * priorities inherited from their procs, and use up cpu time.
 */
void	sched_exit_thread(struct thread *td, struct thread *child);
void	sched_fork_thread(struct thread *td, struct thread *child);
void	sched_lend_prio(struct thread *td, u_char prio);
void	sched_lend_user_prio(struct thread *td, u_char pri);
fixpt_t	sched_pctcpu(struct thread *td);
void	sched_prio(struct thread *td, u_char prio);
void	sched_sleep(struct thread *td);
void	sched_switch(struct thread *td, struct thread *newtd, int flags);
void	sched_throw(struct thread *td);
void	sched_unlend_prio(struct thread *td, u_char prio);
void	sched_unlend_user_prio(struct thread *td, u_char pri);
void	sched_user_prio(struct thread *td, u_char prio);
void	sched_userret(struct thread *td);
void	sched_wakeup(struct thread *td);

/*
 * Threads are moved on and off of run queues
 */
void	sched_add(struct thread *td, int flags);
void	sched_clock(struct thread *td);
void	sched_rem(struct thread *td);
void	sched_tick(void);
void	sched_relinquish(struct thread *td);
struct thread *sched_choose(void);
void	sched_idletd(void *);

/*
 * Binding makes cpu affinity permanent while pinning is used to temporarily
 * hold a thread on a particular CPU.
 */
void	sched_bind(struct thread *td, int cpu);
static __inline void sched_pin(void);
void	sched_unbind(struct thread *td);
static __inline void sched_unpin(void);
int	sched_is_bound(struct thread *td);

/*
 * These procedures tell the process data structure allocation code how
 * many bytes to actually allocate.
 */
int	sched_sizeof_proc(void);
int	sched_sizeof_thread(void);

static __inline void
sched_pin(void)
{
	curthread->td_pinned++;
}

static __inline void
sched_unpin(void)
{
	curthread->td_pinned--;
}

/* sched_add arguments (formerly setrunqueue) */
#define	SRQ_BORING	0x0000		/* No special circumstances. */
#define	SRQ_YIELDING	0x0001		/* We are yielding (from mi_switch). */
#define	SRQ_OURSELF	0x0002		/* It is ourself (from mi_switch). */
#define	SRQ_INTR	0x0004		/* It is probably urgent. */
#define	SRQ_PREEMPTED	0x0008		/* has been preempted.. be kind */
#define	SRQ_BORROWING	0x0010		/* Priority updated due to prio_lend */

/* Switch stats. */
#ifdef SCHED_STATS
extern long switch_preempt;
extern long switch_owepreempt;
extern long switch_turnstile;
extern long switch_sleepq;
extern long switch_sleepqtimo;
extern long switch_relinquish;
extern long switch_needresched;
#define SCHED_STAT_INC(var)     atomic_add_long(&(var), 1)
#else
#define SCHED_STAT_INC(var)
#endif

/* temporarily here */
void schedinit(void);
void sched_newproc(struct proc *p, struct thread *td);
void sched_newthread(struct thread *td);
#endif /* _KERNEL */

/* POSIX 1003.1b Process Scheduling */

/*
 * POSIX scheduling policies
 */
#define SCHED_FIFO      1
#define SCHED_OTHER     2
#define SCHED_RR        3

struct sched_param {
        int     sched_priority;
};

/*
 * POSIX scheduling declarations for userland.
 */
#ifndef _KERNEL
#include <sys/cdefs.h>
#include <sys/_types.h>

#ifndef _PID_T_DECLARED
typedef __pid_t         pid_t;
#define _PID_T_DECLARED
#endif

struct timespec;

__BEGIN_DECLS
int     sched_get_priority_max(int);
int     sched_get_priority_min(int);
int     sched_getparam(pid_t, struct sched_param *);
int     sched_getscheduler(pid_t);
int     sched_rr_get_interval(pid_t, struct timespec *);
int     sched_setparam(pid_t, const struct sched_param *);
int     sched_setscheduler(pid_t, int, const struct sched_param *);
int     sched_yield(void);
__END_DECLS

#endif
#endif /* !_SCHED_H_ */

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

#ifndef _SYS_SCHED_H_
#define	_SYS_SCHED_H_

/*
 * General scheduling info.
 */
int	sched_rr_interval(void);
int	sched_runnable(void);

/* 
 * Proc related scheduling hooks.
 */
void	sched_exit(struct proc *p, struct proc *child);
void	sched_fork(struct proc *p, struct proc *child);

/*
 * KSE Groups contain scheduling priority information.  They record the
 * behavior of groups of KSEs and threads.
 */
void	sched_class(struct ksegrp *kg, int class);
void	sched_exit_ksegrp(struct ksegrp *kg, struct ksegrp *child);
void	sched_fork_ksegrp(struct ksegrp *kg, struct ksegrp *child);
void	sched_nice(struct ksegrp *kg, int nice);

/*
 * Threads are switched in and out, block on resources, have temporary
 * priorities inherited from their ksegs, and use up cpu time.
 */
void	sched_exit_thread(struct thread *td, struct thread *child);
void	sched_fork_thread(struct thread *td, struct thread *child);
fixpt_t	sched_pctcpu(struct thread *td);
void	sched_prio(struct thread *td, u_char prio);
void	sched_sleep(struct thread *td, u_char prio);
void	sched_switch(struct thread *td);
void	sched_userret(struct thread *td);
void	sched_wakeup(struct thread *td);

/*
 * Threads are moved on and off of run queues
 */
void	sched_add(struct thread *td);
struct kse *sched_choose(void);		/* XXX Should be thread * */
void	sched_clock(struct thread *td);
void	sched_rem(struct thread *td);

/*
 * Binding makes cpu affinity permanent while pinning is used to temporarily
 * hold a thread on a particular CPU.
 */
void	sched_bind(struct thread *td, int cpu);
static __inline void sched_pin(void);
void	sched_unbind(struct thread *td);
static __inline void sched_unpin(void);

/*
 * These interfaces will eventually be removed.
 */
void	sched_exit_kse(struct kse *ke, struct kse *child);
void	sched_fork_kse(struct kse *ke, struct kse *child);

/*
 * These procedures tell the process data structure allocation code how
 * many bytes to actually allocate.
 */
int	sched_sizeof_kse(void);
int	sched_sizeof_ksegrp(void);
int	sched_sizeof_proc(void);
int	sched_sizeof_thread(void);

extern struct ke_sched *kse0_sched;
extern struct kg_sched *ksegrp0_sched;
extern struct p_sched *proc0_sched;
extern struct td_sched *thread0_sched;

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

#endif /* !_SYS_SCHED_H_ */

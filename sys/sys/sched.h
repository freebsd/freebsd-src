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
 * KSE Groups contain scheduling priority information.  They record the
 * behavior of groups of KSEs and threads.
 */
void	sched_exit(struct ksegrp *kg, struct ksegrp *child);
void	sched_fork(struct ksegrp *kg, struct ksegrp *child);
void	sched_nice(struct ksegrp *kg, int nice);
void	sched_prio(struct thread *td, u_char prio);
void	sched_userret(struct thread *td);

/*
 * Threads are switched in and out, block on resources, and have temporary
 * priorities inherited from their ksegs.
 */
void	sched_clock(struct thread *td);
void	sched_sleep(struct thread *td, u_char prio);
void	sched_switchin(struct thread *td);
void	sched_switchout(struct thread *td);
void	sched_wakeup(struct thread *td);

/*
 * KSEs are moved on and off of run queues.
 */
void	sched_add(struct kse *ke);
void	sched_rem(struct kse *ke);
struct kse *sched_choose(void);

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

#endif /* !_SYS_SCHED_H_ */

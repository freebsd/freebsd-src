/*-
 * Copyright (c) 2000 Jake Burkholder <jake@freebsd.org>.
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

#ifndef	_SYS_CONDVAR_H_
#define	_SYS_CONDVAR_H_

#ifndef	LOCORE
#include <sys/queue.h>

struct mtx;
struct proc;

TAILQ_HEAD(cv_waitq, proc);

/*
 * Condition variable.
 */
struct cv {
	struct cv_waitq	cv_waitq;	/* Queue of condition waiters. */
	struct mtx	*cv_mtx;	/*
					 * Mutex passed in by cv_*wait*(),
					 * currently only used for CV_DEBUG.
					 */
	const char	*cv_description;
};

#ifdef _KERNEL
void	cv_init(struct cv *cvp, const char *desc);
void	cv_destroy(struct cv *cvp);

void	cv_wait(struct cv *cvp, struct mtx *mp);
int	cv_wait_sig(struct cv *cvp, struct mtx *mp);
int	cv_timedwait(struct cv *cvp, struct mtx *mp, int timo);
int	cv_timedwait_sig(struct cv *cvp, struct mtx *mp, int timo);

void	cv_signal(struct cv *cvp);
void	cv_signal_drop(struct cv *cvp, struct mtx *mp);
void	cv_broadcast(struct cv *cvp);
void	cv_broadcast_drop(struct cv *cvp, struct mtx *mp);

void	cv_waitq_remove(struct proc *p);

#define	cv_waitq_empty(cvp)	(TAILQ_EMPTY(&cvp->cv_waitq))
#define	cv_wmesg(cvp)		((cvp)->cv_description)

#endif	/* _KERNEL */
#endif	/* !LOCORE */
#endif	/* _SYS_CONDVAR_H_ */

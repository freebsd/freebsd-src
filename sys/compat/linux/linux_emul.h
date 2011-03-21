/*-
 * Copyright (c) 2006 Roman Divacky
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#ifndef _LINUX_EMUL_H_
#define	_LINUX_EMUL_H_

#define EMUL_SHARED_HASXSTAT	0x01

struct linux_emuldata_shared {
	int	refs;
	int	flags;
	int	xstat;
	pid_t	group_pid;

	LIST_HEAD(, linux_emuldata) threads; /* head of list of linux threads */
};

/*
 * modeled after similar structure in NetBSD
 * this will be extended as we need more functionality
 */
struct linux_emuldata {
	pid_t	pid;

	int    *child_set_tid;	/* in clone(): Child's TID to set on clone */
	int    *child_clear_tid;/* in clone(): Child's TID to clear on exit */

	struct linux_emuldata_shared *shared;

	int	pdeath_signal;		/* parent death signal */
	int	flags;			/* different emuldata flags */

	struct	linux_robust_list_head	*robust_futexes;

	LIST_ENTRY(linux_emuldata) threads;	/* list of linux threads */
};

struct linux_emuldata	*em_find(struct proc *, int locked);

#define	EMUL_LOCK(l)		mtx_lock(l)
#define	EMUL_UNLOCK(l)		mtx_unlock(l)

#define	EMUL_SHARED_RLOCK(l)	sx_slock(l)
#define	EMUL_SHARED_RUNLOCK(l)	sx_sunlock(l)
#define	EMUL_SHARED_WLOCK(l)	sx_xlock(l)
#define	EMUL_SHARED_WUNLOCK(l)	sx_xunlock(l)

/* for em_find use */
#define	EMUL_DOLOCK		1
#define	EMUL_DONTLOCK		0

/* emuldata flags */
#define	LINUX_XDEPR_REQUEUEOP	0x00000001	/* uses deprecated
						   futex REQUEUE op*/

int	linux_proc_init(struct thread *, pid_t, int);
void	linux_proc_exit(void *, struct proc *);
void	linux_schedtail(struct thread *);
void	linux_proc_exec(void *, struct proc *, struct image_params *);
void	linux_kill_threads(struct thread *, int);

extern struct sx	emul_shared_lock;
extern struct mtx	emul_lock;

#endif	/* !_LINUX_EMUL_H_ */

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2006 Roman Divacky
 * All rights reserved.
 * Copyright (c) 2013 Dmitry Chagin <dchagin@FreeBSD.org>
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

#ifndef _LINUX_EMUL_H_
#define	_LINUX_EMUL_H_

struct image_params;

/*
 * modeled after similar structure in NetBSD
 * this will be extended as we need more functionality
 */
struct linux_emuldata {
	int    *child_set_tid;	/* in clone(): Child's TID to set on clone */
	int    *child_clear_tid;/* in clone(): Child's TID to clear on exit */

	int	flags;			/* thread emuldata flags */
	int	em_tid;			/* thread id */

	struct	linux_robust_list_head	*robust_futexes;
};

struct linux_emuldata	*em_find(struct thread *);

void	linux_proc_init(struct thread *, struct thread *, bool);
void	linux_on_exit(struct proc *);
void	linux_schedtail(struct thread *);
int	linux_on_exec(struct proc *, struct image_params *);
void	linux_thread_dtor(struct thread *);
int	linux_common_execve(struct thread *, struct image_args *);

/* process emuldata flags */
#define	LINUX_XDEPR_REQUEUEOP	0x00000001	/* uses deprecated
						   futex REQUEUE op*/
#define	LINUX_XUNSUP_EPOLL	0x00000002	/* unsupported epoll events */
#define	LINUX_XUNSUP_FUTEXPIOP	0x00000004	/* uses unsupported pi futex */

struct linux_pemuldata {
	uint32_t	flags;		/* process emuldata flags */
	struct sx	pem_sx;		/* lock for this struct */
	uint32_t	persona;	/* process execution domain */
	uint32_t	ptrace_flags;	/* used by ptrace(2) */
	uint32_t	oom_score_adj;	/* /proc/self/oom_score_adj */
	uint32_t	so_timestamp;	/* requested timeval */
	uint32_t	so_timestampns;	/* requested timespec */
};

#define	LINUX_PEM_XLOCK(p)	sx_xlock(&(p)->pem_sx)
#define	LINUX_PEM_XUNLOCK(p)	sx_xunlock(&(p)->pem_sx)
#define	LINUX_PEM_SLOCK(p)	sx_slock(&(p)->pem_sx)
#define	LINUX_PEM_SUNLOCK(p)	sx_sunlock(&(p)->pem_sx)

struct linux_pemuldata	*pem_find(struct proc *);

#endif	/* !_LINUX_EMUL_H_ */

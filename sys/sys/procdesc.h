/*-
 * Copyright (c) 2009 Robert N. M. Watson
 * All rights reserved.
 *
 * WARNING: THIS IS EXPERIMENTAL SECURITY SOFTWARE THAT MUST NOT BE RELIED
 * ON IN PRODUCTION SYSTEMS.  IT WILL BREAK YOUR SOFTWARE IN NEW AND
 * UNEXPECTED WAYS.
 *
 * This software was developed at the University of Cambridge Computer
 * Laboratory with support from a grant from Google, Inc.
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

#ifndef _SYS_PROCDESC_H_
#define	_SYS_PROCDESC_H_

#ifdef _KERNEL
#include <sys/selinfo.h>	/* struct selinfo */
#include <sys/_lock.h>
#include <sys/_mutex.h>

/*-
 * struct procdesc describes a process descriptor, and essentially consists
 * of two pointers -- one to the file descriptor, and one to the process.
 * When both become NULL, the process descriptor will be freed.  An important
 * invariant is that there is only ever one process descriptor for a process,
 * so a single file pointer will suffice.
 *
 * Locking key:
 * (c) - Constant after initial setup.
 * (p) - Protected by the process descriptor mutex.
 * (r) - Atomic eference count.
 * (s) - Protected by selinfo.
 * (t) - Protected by the proctree_lock
 */
struct proc;
struct sigio;
struct procdesc {
	/*
	 * Basic process descriptor state: the process, a cache of its pid to
	 * satisfy queries after the process exits, and process descriptor
	 * refcount.
	 */
	struct proc	*pd_proc;		/* (t) Process. */
	pid_t		 pd_pid;		/* (c) Cached pid. */
	u_int		 pd_refcount;		/* (r) Reference count. */

	/*
	 * In-flight data and notification of events.
	 */
	int		 pd_flags;		/* (p) PD_ flags. */
	struct selinfo	 pd_selinfo;		/* (p) Event notification. */
	struct mtx	 pd_lock;		/* Protect data + events. */
};

/*
 * Locking macros for the procdesc itself.
 */
#define	PROCDESC_LOCK_DESTROY(pd)	mtx_destroy(&(pd)->pd_lock)
#define	PROCDESC_LOCK_INIT(pd)	mtx_init(&(pd)->pd_lock, "procdesc", NULL, \
				    MTX_DEF)
#define	PROCDESC_LOCK(pd)	mtx_lock(&(pd)->pd_lock)
#define	PROCDESC_UNLOCK(pd)	mtx_unlock(&(pd)->pd_lock)

/*
 * Flags for the pd_flags field.
 */
#define	PD_CLOSED	0x00000001	/* Descriptor has closed. */
#define	PD_SELECTED	0x00000002	/* Issue selwakeup(). */
#define	PD_EXITED	0x00000004	/* Process exited. */

/*
 * In-kernel interfaces to process descriptors.
 */
int	 procdesc_exit(struct proc *p);
int	 procdesc_find(struct thread *td, int fd, cap_rights_t rights,
	    struct proc **p);
void	 procdesc_new(struct proc *p, struct file *fp_procdesc);
pid_t	 procdesc_pid(struct file *fp_procdesc);
void	 procdesc_reap(struct proc *p);

#else /* !_KERNEL */

/*
 * Process descriptor system calls.
 */
struct rusage;
int	pdfork(int *fdp);
int	pdkill(int fd, int signum);
int	pdgetpid(int fd, pid_t *pidp);
int	pdwait4(int fd, int *status, int options, struct rusage *rusage);

#endif /* _KERNEL */

#endif /* !_SYS_PROCDESC_H_ */

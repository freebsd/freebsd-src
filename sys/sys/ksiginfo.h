/*-
 * Copyright (c) 2002 New Gold Technology.  All rights reserved.
 * Copyright (c) 2002 Juli Mallett.  All rights reserved.
 *
 * This software was written by Juli Mallett <jmallett@FreeBSD.org> for the
 * FreeBSD project.  Redistribution and use in source and binary forms, with
 * or without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistribution of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistribution in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * $FreeBSD$
 */

#ifndef	_SYS_KSIGINFO_H_
#define	_SYS_KSIGINFO_H_

#ifndef	_KERNEL
#error "no user-serviceable parts inside"
#endif

#include <sys/malloc.h>
#include <sys/signal.h>

/*
 * Structures and prototypes for working with the in-kernel representation
 * of pending signals, and all the information we have about them.
 */

MALLOC_DECLARE(M_KSIGINFO);

/*
 * This is pushed to userland in the form of a siginfo_t, which is POSIX
 * defined.  This is for in-kernel representations only, and has no ABI
 * consumers.
 */
struct ksiginfo {
	TAILQ_ENTRY(ksiginfo)	 ksi_queue;	/* Entry in the signal queue. */
	void			*ksi_addr;	/* [Fault] address. */
	int			 ksi_code;	/* [Trap] code. */
	int			 ksi_errno;	/* Error number. */
	int			 ksi_signo;	/* Signal number. */
	int			 ksi_status;	/* Exit status (SIGCHLD). */
	uid_t			 ksi_ruid;	/* Real UID of sender. */
	pid_t			 ksi_pid;	/* PID of sender. */
};

struct proc;

__BEGIN_DECLS;
int ksiginfo_alloc(struct ksiginfo **, int);
int ksiginfo_dequeue(struct ksiginfo **, struct proc *, int);
int ksiginfo_destroy(struct ksiginfo **);
int ksiginfo_to_siginfo_t(struct ksiginfo *, siginfo_t *);
int ksiginfo_to_sigset_t(struct proc *, sigset_t *);
int signal_add(struct proc *, struct ksiginfo *, int);
int signal_delete(struct proc *, struct ksiginfo *, int);
int signal_delete_mask(struct proc *, int);
int signal_pending(struct proc *);
int signal_queued(struct proc *, int);
int signal_queued_mask(struct proc *, sigset_t);
__END_DECLS;

#endif /* !_SYS_KSIGINFO_H_ */

/* $FreeBSD$ */
/*	$NetBSD: sysv_ipc.c,v 1.7 1994/06/29 06:33:11 cgd Exp $	*/

/*
 * Copyright (c) 1994 Herb Peyerl <hpeyerl@novatel.ca>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Herb Peyerl.
 * 4. The name of Herb Peyerl may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

#include "opt_sysvipc.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/proc.h>
#include <sys/ucred.h>

void (*semexit_hook)(struct proc *) = NULL;
void (*shmfork_hook)(struct proc *, struct proc *) = NULL;
void (*shmexit_hook)(struct proc *) = NULL;

/* called from kern_exit.c */
void
semexit(p)
	struct proc *p;
{

	if (semexit_hook != NULL)
		semexit_hook(p);
	return;
}

/* called from kern_fork.c */
void
shmfork(p1, p2)
	struct proc *p1, *p2;
{

	if (shmfork_hook != NULL)
		shmfork_hook(p1, p2);
	return;
}

/* called from kern_exit.c */
void
shmexit(p)
	struct proc *p;
{

	if (shmexit_hook != NULL)
		shmexit_hook(p);
	return;
}

/*
 * Check for ipc permission
 */

int
ipcperm(td, perm, mode)
	struct thread *td;
	struct ipc_perm *perm;
	int mode;
{
	struct proc *p = td->td_proc;
	struct ucred *cred = p->p_ucred;

	/* Check for user match. */
	if (cred->cr_uid != perm->cuid && cred->cr_uid != perm->uid) {
		if (mode & IPC_M)
			return (suser_xxx(p->p_ucred, NULL, 0) == 0 ? 0 :
			    EPERM);
		/* Check for group match. */
		mode >>= 3;
		if (!groupmember(perm->gid, cred) &&
		    !groupmember(perm->cgid, cred))
			/* Check for `other' match. */
			mode >>= 3;
	}

	if (mode & IPC_M)
		return (0);
	return ((mode & perm->mode) == mode ||
	    suser_xxx(p->p_ucred, NULL, 0) == 0 ? 0 : EACCES);
}

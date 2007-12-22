/*	$NetBSD: sysv_ipc.c,v 1.7 1994/06/29 06:33:11 cgd Exp $	*/
/*-
 * Copyright (c) 1994 Herb Peyerl <hpeyerl@novatel.ca>
 * Copyright (c) 2006 nCircle Network Security, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert N. M. Watson for the TrustedBSD
 * Project under contract to nCircle Network Security, Inc.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_sysvipc.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/ucred.h>

void (*shmfork_hook)(struct proc *, struct proc *) = NULL;
void (*shmexit_hook)(struct vmspace *) = NULL;

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
shmexit(struct vmspace *vm)
{

	if (shmexit_hook != NULL)
		shmexit_hook(vm);
	return;
}

/*
 * Check for IPC permission.
 *
 * Note: The MAC Framework does not require any modifications to the
 * ipcperm() function, as access control checks are performed throughout the
 * implementation of each primitive.  Those entry point calls complement the
 * ipcperm() discertionary checks.  Unlike file system discretionary access
 * control, the original create of an object is given the same rights as the
 * current owner.
 */
int
ipcperm(struct thread *td, struct ipc_perm *perm, int acc_mode)
{
	struct ucred *cred = td->td_ucred;
	int error, obj_mode, dac_granted, priv_granted;

	dac_granted = 0;
	if (cred->cr_uid == perm->cuid || cred->cr_uid == perm->uid) {
		obj_mode = perm->mode;
		dac_granted |= IPC_M;
	} else if (groupmember(perm->gid, cred) ||
	    groupmember(perm->cgid, cred)) {
		obj_mode = perm->mode;
		obj_mode <<= 3;
	} else {
		obj_mode = perm->mode;
		obj_mode <<= 6;
	}

	/*
	 * While the System V IPC permission model allows IPC_M to be
	 * granted, as part of the mode, our implementation requires
	 * privilege to adminster the object if not the owner or creator.
	 */
#if 0
	if (obj_mode & IPC_M)
		dac_granted |= IPC_M;
#endif
	if (obj_mode & IPC_R)
		dac_granted |= IPC_R;
	if (obj_mode & IPC_W)
		dac_granted |= IPC_W;

	/*
	 * Simple case: all required rights are granted by DAC.
	 */
	if ((dac_granted & acc_mode) == acc_mode)
		return (0);

	/*
	 * Privilege is required to satisfy the request.
	 */
	priv_granted = 0;
	if ((acc_mode & IPC_M) && !(dac_granted & IPC_M)) {
		error = priv_check(td, PRIV_IPC_ADMIN);
		if (error == 0)
			priv_granted |= IPC_M;
	}

	if ((acc_mode & IPC_R) && !(dac_granted & IPC_R)) {
		error = priv_check(td, PRIV_IPC_READ);
		if (error == 0)
			priv_granted |= IPC_R;
	}

	if ((acc_mode & IPC_W) && !(dac_granted & IPC_W)) {
		error = priv_check(td, PRIV_IPC_WRITE);
		if (error == 0)
			priv_granted |= IPC_W;
	}

	if (((dac_granted | priv_granted) & acc_mode) == acc_mode)
		return (0);
	else
		return (EACCES);
}

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
#include <sys/ipc.h>
#include <sys/proc.h>
#include <sys/ucred.h>

#if defined(SYSVSEM) || defined(SYSVSHM) || defined(SYSVMSG)

/*
 * Check for ipc permission
 */

int
ipcperm(p, perm, mode)
	struct proc *p;
	struct ipc_perm *perm;
	int mode;
{
	struct ucred *cred = p->p_ucred;

	/* Check for user match. */
	if (cred->cr_uid != perm->cuid && cred->cr_uid != perm->uid) {
		if (mode & IPC_M)
			return (suser(p) == 0 ? 0 : EPERM);
		/* Check for group match. */
		mode >>= 3;
		if (!groupmember(perm->gid, cred) &&
		    !groupmember(perm->cgid, cred))
			/* Check for `other' match. */
			mode >>= 3;
	}

	if (mode & IPC_M)
		return (0);
	return ((mode & perm->mode) == mode || suser(p) == 0 ? 0 : EACCES);
}

#endif /* defined(SYSVSEM) || defined(SYSVSHM) || defined(SYSVMSG) */


#if !defined(SYSVSEM) || !defined(SYSVSHM) || !defined(SYSVMSG)

#include <sys/proc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/syslog.h>
#include <sys/sysproto.h>
#include <sys/systm.h>

static void sysv_nosys __P((struct proc *p, char *s));

static void 
sysv_nosys(p, s)
	struct proc *p;
	char *s;
{
	log(LOG_ERR, "cmd %s pid %d tried to use non-present %s\n",
			p->p_comm, p->p_pid, s);
}

#if !defined(SYSVSEM)

/*
 * SYSVSEM stubs
 */

int
semsys(p, uap)
	struct proc *p;
	struct semsys_args *uap;
{
	sysv_nosys(p, "SYSVSEM");
	return nosys(p, (struct nosys_args *)uap);
};

int
__semctl(p, uap)
	struct proc *p;
	register struct __semctl_args *uap;
{
	sysv_nosys(p, "SYSVSEM");
	return nosys(p, (struct nosys_args *)uap);
};

int
semget(p, uap)
	struct proc *p;
	register struct semget_args *uap;
{
	sysv_nosys(p, "SYSVSEM");
	return nosys(p, (struct nosys_args *)uap);
};

int
semop(p, uap)
	struct proc *p;
	register struct semop_args *uap;
{
	sysv_nosys(p, "SYSVSEM");
	return nosys(p, (struct nosys_args *)uap);
};

/* called from kern_exit.c */
void
semexit(p)
	struct proc *p;
{
	return;
}

#endif /* !defined(SYSVSEM) */


#if !defined(SYSVMSG)

/*
 * SYSVMSG stubs
 */

int
msgsys(p, uap)
	struct proc *p;
	/* XXX actually varargs. */
	struct msgsys_args *uap;
{
	sysv_nosys(p, "SYSVMSG");
	return nosys(p, (struct nosys_args *)uap);
};

int
msgctl(p, uap)
	struct proc *p;
	register struct msgctl_args *uap;
{
	sysv_nosys(p, "SYSVMSG");
	return nosys(p, (struct nosys_args *)uap);
};

int
msgget(p, uap)
	struct proc *p;
	register struct msgget_args *uap;
{
	sysv_nosys(p, "SYSVMSG");
	return nosys(p, (struct nosys_args *)uap);
};

int
msgsnd(p, uap)
	struct proc *p;
	register struct msgsnd_args *uap;
{
	sysv_nosys(p, "SYSVMSG");
	return nosys(p, (struct nosys_args *)uap);
};

int
msgrcv(p, uap)
	struct proc *p;
	register struct msgrcv_args *uap;
{
	sysv_nosys(p, "SYSVMSG");
	return nosys(p, (struct nosys_args *)uap);
};

#endif /* !defined(SYSVMSG) */


#if !defined(SYSVSHM)

/*
 * SYSVSHM stubs
 */

int
shmdt(p, uap)
	struct proc *p;
	struct shmdt_args *uap;
{
	sysv_nosys(p, "SYSVSHM");
	return nosys(p, (struct nosys_args *)uap);
};

int
shmat(p, uap)
	struct proc *p;
	struct shmat_args *uap;
{
	sysv_nosys(p, "SYSVSHM");
	return nosys(p, (struct nosys_args *)uap);
};

int
shmctl(p, uap)
	struct proc *p;
	struct shmctl_args *uap;
{
	sysv_nosys(p, "SYSVSHM");
	return nosys(p, (struct nosys_args *)uap);
};

int
shmget(p, uap)
	struct proc *p;
	struct shmget_args *uap;
{
	sysv_nosys(p, "SYSVSHM");
	return nosys(p, (struct nosys_args *)uap);
};

int
shmsys(p, uap)
	struct proc *p;
	/* XXX actually varargs. */
	struct shmsys_args *uap;
{
	sysv_nosys(p, "SYSVSHM");
	return nosys(p, (struct nosys_args *)uap);
};

/* called from kern_fork.c */
void
shmfork(p1, p2)
	struct proc *p1, *p2;
{
	return;
}

/* called from kern_exit.c */
void
shmexit(p)
	struct proc *p;
{
	return;
}

#endif /* !defined(SYSVSHM) */

#endif /* !defined(SYSVSEM) || !defined(SYSVSHM) || !defined(SYSVMSG) */

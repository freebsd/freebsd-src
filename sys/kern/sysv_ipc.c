/*	$FreeBSD$ */
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
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/sysproto.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#if defined(SYSVSEM) || defined(SYSVSHM) || defined(SYSVMSG)

/*
 * Check for ipc permission
 */

int
ipcperm(cred, perm, mode)
	struct ucred *cred;
	struct ipc_perm *perm;
	int mode;
{

	if (cred->cr_uid == 0)
		return (0);

	/* Check for user match. */
	if (cred->cr_uid != perm->cuid && cred->cr_uid != perm->uid) {
		if (mode & IPC_M)
			return (EPERM);
		/* Check for group match. */
		mode >>= 3;
		if (!groupmember(perm->gid, cred) &&
		    !groupmember(perm->cgid, cred))
			/* Check for `other' match. */
			mode >>= 3;
	}

	if (mode & IPC_M)
		return (0);
	return ((mode & perm->mode) == mode ? 0 : EACCES);
}

#endif /* defined(SYSVSEM) || defined(SYSVSHM) || defined(SYSVMSG) */


#if !defined(SYSVSEM) || !defined(SYSVSHM) || !defined(SYSVMSG)

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
semsys(p, uap, retval)
	struct proc *p;
	struct semsys_args *uap;
	int *retval;
{
	sysv_nosys(p, "SYSVSEM");
	return nosys(p, (struct nosys_args *)uap, retval);
};

int
semconfig(p, uap, retval)
	struct proc *p;
	struct semconfig_args *uap;
	int *retval;
{
	sysv_nosys(p, "SYSVSEM");
	return nosys(p, (struct nosys_args *)uap, retval);
};

int
__semctl(p, uap, retval)
	struct proc *p;
	register struct __semctl_args *uap;
	int *retval;
{
	sysv_nosys(p, "SYSVSEM");
	return nosys(p, (struct nosys_args *)uap, retval);
};

int
semget(p, uap, retval)
	struct proc *p;
	register struct semget_args *uap;
	int *retval;
{
	sysv_nosys(p, "SYSVSEM");
	return nosys(p, (struct nosys_args *)uap, retval);
};

int
semop(p, uap, retval)
	struct proc *p;
	register struct semop_args *uap;
	int *retval;
{
	sysv_nosys(p, "SYSVSEM");
	return nosys(p, (struct nosys_args *)uap, retval);
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
msgsys(p, uap, retval)
	struct proc *p;
	/* XXX actually varargs. */
	struct msgsys_args *uap;
	int *retval;
{
	sysv_nosys(p, "SYSVMSG");
	return nosys(p, (struct nosys_args *)uap, retval);
};

int
msgctl(p, uap, retval)
	struct proc *p;
	register struct msgctl_args *uap;
	int *retval;
{
	sysv_nosys(p, "SYSVMSG");
	return nosys(p, (struct nosys_args *)uap, retval);
};

int
msgget(p, uap, retval)
	struct proc *p;
	register struct msgget_args *uap;
	int *retval;
{
	sysv_nosys(p, "SYSVMSG");
	return nosys(p, (struct nosys_args *)uap, retval);
};

int
msgsnd(p, uap, retval)
	struct proc *p;
	register struct msgsnd_args *uap;
	int *retval;
{
	sysv_nosys(p, "SYSVMSG");
	return nosys(p, (struct nosys_args *)uap, retval);
};

int
msgrcv(p, uap, retval)
	struct proc *p;
	register struct msgrcv_args *uap;
	int *retval;
{
	sysv_nosys(p, "SYSVMSG");
	return nosys(p, (struct nosys_args *)uap, retval);
};

#endif /* !defined(SYSVMSG) */


#if !defined(SYSVSHM)

/*
 * SYSVSHM stubs
 */

int
shmdt(p, uap, retval)
	struct proc *p;
	struct shmdt_args *uap;
	int *retval;
{
	sysv_nosys(p, "SYSVSHM");
	return nosys(p, (struct nosys_args *)uap, retval);
};

int
shmat(p, uap, retval)
	struct proc *p;
	struct shmat_args *uap;
	int *retval;
{
	sysv_nosys(p, "SYSVSHM");
	return nosys(p, (struct nosys_args *)uap, retval);
};

int
shmctl(p, uap, retval)
	struct proc *p;
	struct shmctl_args *uap;
	int *retval;
{
	sysv_nosys(p, "SYSVSHM");
	return nosys(p, (struct nosys_args *)uap, retval);
};

int
shmget(p, uap, retval)
	struct proc *p;
	struct shmget_args *uap;
	int *retval;
{
	sysv_nosys(p, "SYSVSHM");
	return nosys(p, (struct nosys_args *)uap, retval);
};

int
shmsys(p, uap, retval)
	struct proc *p;
	/* XXX actually varargs. */
	struct shmsys_args *uap;
	int *retval;
{
	sysv_nosys(p, "SYSVSHM");
	return nosys(p, (struct nosys_args *)uap, retval);
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

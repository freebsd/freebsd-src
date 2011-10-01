/*-
 * Copyright (c) 1995 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*-
 * Portions of this code have been derived from software contributed
 * to the FreeBSD Project by Mark Newton.
 *
 * Copyright (c) 1999 Mark Newton
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
 * XXX- This code is presently a no-op on FreeBSD (and isn't compiled due
 * to preprocessor conditionals).  A nice project for a kernel hacking 
 * novice might be to MakeItGo, but I have more important fish to fry
 * at present.
 *
 *	Derived from: $NetBSD: svr4_ipc.c,v 1.7 1998/10/19 22:43:00 tron Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_sysvipc.h"

#include <sys/param.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/proc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/time.h>

#include <compat/svr4/svr4.h>
#include <compat/svr4/svr4_types.h>
#include <compat/svr4/svr4_signal.h>
#include <compat/svr4/svr4_proto.h>
#include <compat/svr4/svr4_util.h>
#include <compat/svr4/svr4_ipc.h>

#if defined(SYSVMSG) || defined(SYSVSHM) || defined(SYSVSEM)
static void svr4_to_bsd_ipc_perm(const struct svr4_ipc_perm *,
				      struct ipc_perm *);
static void bsd_to_svr4_ipc_perm(const struct ipc_perm *,
				      struct svr4_ipc_perm *);
#endif

#ifdef SYSVSEM
static void bsd_to_svr4_semid_ds(const struct semid_ds *,
				      struct svr4_semid_ds *);
static void svr4_to_bsd_semid_ds(const struct svr4_semid_ds *,
				      struct semid_ds *);
static int svr4_semop(struct thread *, void *);
static int svr4_semget(struct thread *, void *);
static int svr4_semctl(struct thread *, void *);
#endif

#ifdef SYSVMSG
static void bsd_to_svr4_msqid_ds(const struct msqid_ds *,
				      struct svr4_msqid_ds *);
static void svr4_to_bsd_msqid_ds(const struct svr4_msqid_ds *,
				      struct msqid_ds *);
static int svr4_msgsnd(struct thread *, void *);
static int svr4_msgrcv(struct thread *, void *);
static int svr4_msgget(struct thread *, void *);
static int svr4_msgctl(struct thread *, void *);
#endif

#ifdef SYSVSHM
static void bsd_to_svr4_shmid_ds(const struct shmid_ds *,
				      struct svr4_shmid_ds *);
static void svr4_to_bsd_shmid_ds(const struct svr4_shmid_ds *,
				      struct shmid_ds *);
static int svr4_shmat(struct thread *, void *);
static int svr4_shmdt(struct thread *, void *);
static int svr4_shmget(struct thread *, void *);
static int svr4_shmctl(struct thread *, void *);
#endif

#if defined(SYSVMSG) || defined(SYSVSHM) || defined(SYSVSEM)

static void
svr4_to_bsd_ipc_perm(spp, bpp)
	const struct svr4_ipc_perm *spp;
	struct ipc_perm *bpp;
{
	bpp->key = spp->key;
	bpp->uid = spp->uid;
	bpp->gid = spp->gid;
	bpp->cuid = spp->cuid;
	bpp->cgid = spp->cgid;
	bpp->mode = spp->mode;
	bpp->seq = spp->seq;
}

static void
bsd_to_svr4_ipc_perm(bpp, spp)
	const struct ipc_perm *bpp;
	struct svr4_ipc_perm *spp;
{
	spp->key = bpp->key;
	spp->uid = bpp->uid;
	spp->gid = bpp->gid;
	spp->cuid = bpp->cuid;
	spp->cgid = bpp->cgid;
	spp->mode = bpp->mode;
	spp->seq = bpp->seq;
}
#endif

#ifdef SYSVSEM
static void
bsd_to_svr4_semid_ds(bds, sds)
	const struct semid_ds *bds;
	struct svr4_semid_ds *sds;
{
	bzero(sds, sizeof(*sds));
	bsd_to_svr4_ipc_perm(&bds->sem_perm, &sds->sem_perm);
	sds->sem_base = (struct svr4_sem *) bds->sem_base;
	sds->sem_nsems = bds->sem_nsems;
	sds->sem_otime = bds->sem_otime;
	sds->sem_ctime = bds->sem_ctime;
}

static void
svr4_to_bsd_semid_ds(sds, bds)
	const struct svr4_semid_ds *sds;
	struct semid_ds *bds;
{
	svr4_to_bsd_ipc_perm(&sds->sem_perm, &bds->sem_perm);
	bds->sem_base = (struct sem *) bds->sem_base;
	bds->sem_nsems = sds->sem_nsems;
	bds->sem_otime = sds->sem_otime;
	bds->sem_ctime = sds->sem_ctime;
}

struct svr4_sys_semctl_args {
	int what;
	int semid;
	int semnum;
	int cmd;
	union semun arg;
};

static int
svr4_semctl(td, v)
	struct thread *td;
	void *v;
{
	struct svr4_sys_semctl_args *uap = v;
	struct svr4_semid_ds ss;
	struct semid_ds bs;
	union semun semun;
	register_t rval;
	int cmd, error;

	switch (uap->cmd) {
	case SVR4_SEM_GETZCNT:
		cmd = GETZCNT;
		break;

	case SVR4_SEM_GETNCNT:
		cmd = GETNCNT;
		break;

	case SVR4_SEM_GETPID:
		cmd = GETPID;
		break;

	case SVR4_SEM_GETVAL:
		cmd = GETVAL;
		break;

	case SVR4_SEM_SETVAL:
		cmd = SETVAL;
		break;

	case SVR4_SEM_GETALL:
		cmd = GETVAL;
		break;

	case SVR4_SEM_SETALL:
		cmd = SETVAL;
		break;

	case SVR4_IPC_STAT:
		cmd = IPC_STAT;
		semun.buf = &bs;
		error = kern_semctl(td, uap->semid, uap->semnum, cmd, &semun,
		    &rval);
		if (error)
                        return (error);
                bsd_to_svr4_semid_ds(&bs, &ss);
		error = copyout(&ss, uap->arg.buf, sizeof(ss));
		if (error == 0)
			td->td_retval[0] = rval;
		return (error);

	case SVR4_IPC_SET:
		cmd = IPC_SET;
		error = copyin(uap->arg.buf, (caddr_t) &ss, sizeof ss);
                if (error)
                        return (error);
                svr4_to_bsd_semid_ds(&ss, &bs);
		semun.buf = &bs;
		return (kern_semctl(td, uap->semid, uap->semnum, cmd, &semun,
		    td->td_retval));

	case SVR4_IPC_RMID:
		cmd = IPC_RMID;
		break;

	default:
		return EINVAL;
	}

	return (kern_semctl(td, uap->semid, uap->semnum, cmd, &uap->arg,
	    td->td_retval));
}

struct svr4_sys_semget_args {
	int what;
	svr4_key_t key;
	int nsems;
	int semflg;
};

static int
svr4_semget(td, v)
	struct thread *td;
	void *v;
{
	struct svr4_sys_semget_args *uap = v;
	struct semget_args ap;

	ap.key = uap->key;
	ap.nsems = uap->nsems;
	ap.semflg = uap->semflg;

	return sys_semget(td, &ap);
}

struct svr4_sys_semop_args {
	int what;
	int semid;
	struct svr4_sembuf * sops;
	u_int nsops;
};

static int
svr4_semop(td, v)
	struct thread *td;
	void *v;
{
	struct svr4_sys_semop_args *uap = v;
	struct semop_args ap;

	ap.semid = uap->semid;
	/* These are the same */
	ap.sops = (struct sembuf *) uap->sops;
	ap.nsops = uap->nsops;

	return sys_semop(td, &ap);
}

int
svr4_sys_semsys(td, uap)
	struct thread *td;
	struct svr4_sys_semsys_args *uap;
{

	DPRINTF(("svr4_semsys(%d)\n", uap->what));

	switch (uap->what) {
	case SVR4_semctl:
		return svr4_semctl(td, uap);
	case SVR4_semget:
		return svr4_semget(td, uap);
	case SVR4_semop:
		return svr4_semop(td, uap);
	default:
		return EINVAL;
	}
}

MODULE_DEPEND(svr4elf, sysvsem, 1, 1, 1);
#endif

#ifdef SYSVMSG
static void
bsd_to_svr4_msqid_ds(bds, sds)
	const struct msqid_ds *bds;
	struct svr4_msqid_ds *sds;
{
	bzero(sds, sizeof(*sds));
	bsd_to_svr4_ipc_perm(&bds->msg_perm, &sds->msg_perm);
	sds->msg_first = (struct svr4_msg *) bds->msg_first;
	sds->msg_last = (struct svr4_msg *) bds->msg_last;
	sds->msg_cbytes = bds->msg_cbytes;
	sds->msg_qnum = bds->msg_qnum;
	sds->msg_qbytes = bds->msg_qbytes;
	sds->msg_lspid = bds->msg_lspid;
	sds->msg_lrpid = bds->msg_lrpid;
	sds->msg_stime = bds->msg_stime;
	sds->msg_rtime = bds->msg_rtime;
	sds->msg_ctime = bds->msg_ctime;
}

static void
svr4_to_bsd_msqid_ds(sds, bds)
	const struct svr4_msqid_ds *sds;
	struct msqid_ds *bds;
{
	svr4_to_bsd_ipc_perm(&sds->msg_perm, &bds->msg_perm);
	bds->msg_first = (struct msg *) sds->msg_first;
	bds->msg_last = (struct msg *) sds->msg_last;
	bds->msg_cbytes = sds->msg_cbytes;
	bds->msg_qnum = sds->msg_qnum;
	bds->msg_qbytes = sds->msg_qbytes;
	bds->msg_lspid = sds->msg_lspid;
	bds->msg_lrpid = sds->msg_lrpid;
	bds->msg_stime = sds->msg_stime;
	bds->msg_rtime = sds->msg_rtime;
	bds->msg_ctime = sds->msg_ctime;
}

struct svr4_sys_msgsnd_args {
	int what;
	int msqid;
	void * msgp;
	size_t msgsz;
	int msgflg;
};

static int
svr4_msgsnd(td, v)
	struct thread *td;
	void *v;
{
	struct svr4_sys_msgsnd_args *uap = v;
	struct msgsnd_args ap;

	ap.msqid = uap->msqid;
	ap.msgp = uap->msgp;
	ap.msgsz = uap->msgsz;
	ap.msgflg = uap->msgflg;

	return sys_msgsnd(td, &ap);
}

struct svr4_sys_msgrcv_args {
	int what;
	int msqid;
	void * msgp;
	size_t msgsz;
	long msgtyp;
	int msgflg;
};

static int
svr4_msgrcv(td, v)
	struct thread *td;
	void *v;
{
	struct svr4_sys_msgrcv_args *uap = v;
	struct msgrcv_args ap;

	ap.msqid = uap->msqid;
	ap.msgp = uap->msgp;
	ap.msgsz = uap->msgsz;
	ap.msgtyp = uap->msgtyp;
	ap.msgflg = uap->msgflg;

	return sys_msgrcv(td, &ap);
}
	
struct svr4_sys_msgget_args {
	int what;
	svr4_key_t key;
	int msgflg;
};

static int
svr4_msgget(td, v)
	struct thread *td;
	void *v;
{
	struct svr4_sys_msgget_args *uap = v;
	struct msgget_args ap;

	ap.key = uap->key;
	ap.msgflg = uap->msgflg;

	return sys_msgget(td, &ap);
}

struct svr4_sys_msgctl_args {
	int what;
	int msqid;
	int cmd;
	struct svr4_msqid_ds * buf;
};

static int
svr4_msgctl(td, v)
	struct thread *td;
	void *v;
{
	struct svr4_sys_msgctl_args *uap = v;
	struct svr4_msqid_ds ss;
	struct msqid_ds bs;
	int error;

	switch (uap->cmd) {
	case SVR4_IPC_STAT:
		error = kern_msgctl(td, uap->msqid, IPC_STAT, &bs);
		if (error)
			return error;
		bsd_to_svr4_msqid_ds(&bs, &ss);
		return copyout(&ss, uap->buf, sizeof ss);

	case SVR4_IPC_SET:
		error = copyin(uap->buf, &ss, sizeof ss);
		if (error)
			return error;
		svr4_to_bsd_msqid_ds(&ss, &bs);
		return (kern_msgctl(td, uap->msqid, IPC_SET, &bs));

	case SVR4_IPC_RMID:
		return (kern_msgctl(td, uap->msqid, IPC_RMID, NULL));

	default:
		return EINVAL;
	}
}

int
svr4_sys_msgsys(td, uap)
	struct thread *td;
	struct svr4_sys_msgsys_args *uap;
{

	DPRINTF(("svr4_msgsys(%d)\n", uap->what));

	switch (uap->what) {
	case SVR4_msgsnd:
		return svr4_msgsnd(td, uap);
	case SVR4_msgrcv:
		return svr4_msgrcv(td, uap);
	case SVR4_msgget:
		return svr4_msgget(td, uap);
	case SVR4_msgctl:
		return svr4_msgctl(td, uap);
	default:
		return EINVAL;
	}
}

MODULE_DEPEND(svr4elf, sysvmsg, 1, 1, 1);
#endif

#ifdef SYSVSHM

static void
bsd_to_svr4_shmid_ds(bds, sds)
	const struct shmid_ds *bds;
	struct svr4_shmid_ds *sds;
{
	bzero(sds, sizeof(*sds));
	bsd_to_svr4_ipc_perm(&bds->shm_perm, &sds->shm_perm);
	sds->shm_segsz = bds->shm_segsz;
	sds->shm_lkcnt = 0;
	sds->shm_lpid = bds->shm_lpid;
	sds->shm_cpid = bds->shm_cpid;
	sds->shm_amp = 0;
	sds->shm_nattch = bds->shm_nattch;
	sds->shm_cnattch = 0;
	sds->shm_atime = bds->shm_atime;
	sds->shm_dtime = bds->shm_dtime;
	sds->shm_ctime = bds->shm_ctime;
}

static void
svr4_to_bsd_shmid_ds(sds, bds)
	const struct svr4_shmid_ds *sds;
	struct shmid_ds *bds;
{
	svr4_to_bsd_ipc_perm(&sds->shm_perm, &bds->shm_perm);
	bds->shm_segsz = sds->shm_segsz;
	bds->shm_lpid = sds->shm_lpid;
	bds->shm_cpid = sds->shm_cpid;
	bds->shm_nattch = sds->shm_nattch;
	bds->shm_atime = sds->shm_atime;
	bds->shm_dtime = sds->shm_dtime;
	bds->shm_ctime = sds->shm_ctime;
}

struct svr4_sys_shmat_args {
	int what;
	int shmid;
	void * shmaddr;
	int shmflg;
};

static int
svr4_shmat(td, v)
	struct thread *td;
	void *v;
{
	struct svr4_sys_shmat_args *uap = v;
	struct shmat_args ap;

	ap.shmid = uap->shmid;
	ap.shmaddr = uap->shmaddr;
	ap.shmflg = uap->shmflg;

	return sys_shmat(td, &ap);
}

struct svr4_sys_shmdt_args {
	int what;
	void * shmaddr;
};

static int
svr4_shmdt(td, v)
	struct thread *td;
	void *v;
{
	struct svr4_sys_shmdt_args *uap = v;
	struct shmdt_args ap;

	ap.shmaddr = uap->shmaddr;

	return sys_shmdt(td, &ap);
}

struct svr4_sys_shmget_args {
	int what;
	key_t key;
	int size;
	int shmflg;
};

static int
svr4_shmget(td, v)
	struct thread *td;
	void *v;
{
	struct svr4_sys_shmget_args *uap = v;
	struct shmget_args ap;

	ap.key = uap->key;
	ap.size = uap->size;
	ap.shmflg = uap->shmflg;

	return sys_shmget(td, &ap);
}

struct svr4_sys_shmctl_args {
	int what;
	int shmid;
	int cmd;
	struct svr4_shmid_ds * buf;
};

int
svr4_shmctl(td, v)
	struct thread *td;
	void *v;
{
	struct svr4_sys_shmctl_args *uap = v;
	struct shmid_ds bs;
	struct svr4_shmid_ds ss;
	size_t bufsize;
	int cmd, error;

	if (uap->buf != NULL) {
		switch (uap->cmd) {
		case SVR4_IPC_SET:
		case SVR4_SHM_LOCK:
		case SVR4_SHM_UNLOCK:
			error = copyin(uap->buf, &ss, sizeof(ss));
			if (error)
				return (error);
			svr4_to_bsd_shmid_ds(&ss, &bs);
			break;
		default:
			return (EINVAL);
		}
	}

	switch (uap->cmd) {
	case SVR4_IPC_STAT:
		cmd = IPC_STAT;
		break;
	case SVR4_IPC_SET:
		cmd = IPC_SET;
		break;
	case SVR4_IPC_RMID:
		cmd = IPC_RMID;
		break;
	case SVR4_SHM_LOCK:
		cmd = SHM_LOCK;
		break;
	case SVR4_SHM_UNLOCK:
		cmd = SHM_UNLOCK;
		break;
	default:
		return (EINVAL);
	}
		
	error = kern_shmctl(td, uap->shmid, cmd, &bs, &bufsize);
	if (error)
		return (error);

	switch (uap->cmd) {
	case SVR4_IPC_STAT:
		if (uap->buf != NULL) {
			bsd_to_svr4_shmid_ds(&bs, &ss);
			error = copyout(&ss, uap->buf, sizeof(ss));
		}
		break;
	}

	return (error);
}

int
svr4_sys_shmsys(td, uap)
	struct thread *td;
	struct svr4_sys_shmsys_args *uap;
{

	DPRINTF(("svr4_shmsys(%d)\n", uap->what));

	switch (uap->what) {
	case SVR4_shmat:
		return svr4_shmat(td, uap);
	case SVR4_shmdt:
		return svr4_shmdt(td, uap);
	case SVR4_shmget:
		return svr4_shmget(td, uap);
	case SVR4_shmctl:
		return svr4_shmctl(td, uap);
	default:
		return ENOSYS;
	}
}

MODULE_DEPEND(svr4elf, sysvshm, 1, 1, 1);
#endif /* SYSVSHM */

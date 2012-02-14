/*-
 * Copyright (c) 1995 Scott Bartram
 * Copyright (c) 1995 Steven Wallace
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/limits.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>

#include <i386/ibcs2/ibcs2_types.h>
#include <i386/ibcs2/ibcs2_signal.h>
#include <i386/ibcs2/ibcs2_proto.h>
#include <i386/ibcs2/ibcs2_util.h>
#include <i386/ibcs2/ibcs2_ipc.h>

#define IBCS2_IPC_RMID	0
#define IBCS2_IPC_SET	1
#define IBCS2_IPC_STAT	2
#define IBCS2_SETVAL	8



static void cvt_msqid2imsqid(struct msqid_ds *, struct ibcs2_msqid_ds *);
static void cvt_imsqid2msqid(struct ibcs2_msqid_ds *, struct msqid_ds *);
#ifdef unused
static void cvt_sem2isem(struct sem *, struct ibcs2_sem *);
static void cvt_isem2sem(struct ibcs2_sem *, struct sem *);
#endif
static void cvt_semid2isemid(struct semid_ds *, struct ibcs2_semid_ds *);
static void cvt_isemid2semid(struct ibcs2_semid_ds *, struct semid_ds *);
static void cvt_shmid2ishmid(struct shmid_ds *, struct ibcs2_shmid_ds *);
static void cvt_ishmid2shmid(struct ibcs2_shmid_ds *, struct shmid_ds *);
static void cvt_perm2iperm(struct ipc_perm *, struct ibcs2_ipc_perm *);
static void cvt_iperm2perm(struct ibcs2_ipc_perm *, struct ipc_perm *);


/*
 * iBCS2 msgsys call
 */

static void
cvt_msqid2imsqid(bp, ibp)
struct msqid_ds *bp;
struct ibcs2_msqid_ds *ibp;
{
	cvt_perm2iperm(&bp->msg_perm, &ibp->msg_perm);
	ibp->msg_first = bp->msg_first;
	ibp->msg_last = bp->msg_last;
	ibp->msg_cbytes = (u_short)bp->msg_cbytes;
	ibp->msg_qnum = (u_short)bp->msg_qnum;
	ibp->msg_qbytes = (u_short)bp->msg_qbytes;
	ibp->msg_lspid = (u_short)bp->msg_lspid;
	ibp->msg_lrpid = (u_short)bp->msg_lrpid;
	ibp->msg_stime = bp->msg_stime;
	ibp->msg_rtime = bp->msg_rtime;
	ibp->msg_ctime = bp->msg_ctime;
	return;
}

static void
cvt_imsqid2msqid(ibp, bp)
struct ibcs2_msqid_ds *ibp;
struct msqid_ds *bp;
{
	cvt_iperm2perm(&ibp->msg_perm, &bp->msg_perm);
	bp->msg_first = ibp->msg_first;
	bp->msg_last = ibp->msg_last;
	bp->msg_cbytes = ibp->msg_cbytes;
	bp->msg_qnum = ibp->msg_qnum;
	bp->msg_qbytes = ibp->msg_qbytes;
	bp->msg_lspid = ibp->msg_lspid;
	bp->msg_lrpid = ibp->msg_lrpid;
	bp->msg_stime = ibp->msg_stime;
	bp->msg_rtime = ibp->msg_rtime;
	bp->msg_ctime = ibp->msg_ctime;
	return;
}

struct ibcs2_msgget_args {
	int what;
	ibcs2_key_t key;
	int msgflg;
};

static int
ibcs2_msgget(struct thread *td, void *v)
{
	struct ibcs2_msgget_args *uap = v;
	struct msgget_args ap;

	ap.key = uap->key;
	ap.msgflg = uap->msgflg;
	return sys_msgget(td, &ap);
}

struct ibcs2_msgctl_args {
	int what;
	int msqid;
	int cmd;
	struct ibcs2_msqid_ds *buf;
};

static int
ibcs2_msgctl(struct thread *td, void *v)
{
	struct ibcs2_msgctl_args *uap = v;
	struct ibcs2_msqid_ds is;
	struct msqid_ds bs;
	int error;

	switch (uap->cmd) {
	case IBCS2_IPC_STAT:
		error = kern_msgctl(td, uap->msqid, IPC_STAT, &bs);
		if (!error) {
			cvt_msqid2imsqid(&bs, &is);
			error = copyout(&is, uap->buf, sizeof(is));
		}
		return (error);
	case IBCS2_IPC_SET:
		error = copyin(uap->buf, &is, sizeof(is));
		if (error)
			return (error);
		cvt_imsqid2msqid(&is, &bs);
		return (kern_msgctl(td, uap->msqid, IPC_SET, &bs));
	case IBCS2_IPC_RMID:
		return (kern_msgctl(td, uap->msqid, IPC_RMID, NULL));
	}
	return (EINVAL);
}

struct ibcs2_msgrcv_args {
	int what;
	int msqid;
	void *msgp;
	size_t msgsz;
	long msgtyp;
	int msgflg;
};

static int
ibcs2_msgrcv(struct thread *td, void *v)
{
	struct ibcs2_msgrcv_args *uap = v;
	struct msgrcv_args ap;

	ap.msqid = uap->msqid;
	ap.msgp = uap->msgp;
	ap.msgsz = uap->msgsz;
	ap.msgtyp = uap->msgtyp;
	ap.msgflg = uap->msgflg;
	return (sys_msgrcv(td, &ap));
}

struct ibcs2_msgsnd_args {
	int what;
	int msqid;
	void *msgp;
	size_t msgsz;
	int msgflg;
};

static int
ibcs2_msgsnd(struct thread *td, void *v)
{
	struct ibcs2_msgsnd_args *uap = v;
	struct msgsnd_args ap;

	ap.msqid = uap->msqid;
	ap.msgp = uap->msgp;
	ap.msgsz = uap->msgsz;
	ap.msgflg = uap->msgflg;
	return (sys_msgsnd(td, &ap));
}

int
ibcs2_msgsys(td, uap)
	struct thread *td;
	struct ibcs2_msgsys_args *uap;
{
	switch (uap->which) {
	case 0:
		return (ibcs2_msgget(td, uap));
	case 1:
		return (ibcs2_msgctl(td, uap));
	case 2:
		return (ibcs2_msgrcv(td, uap));
	case 3:
		return (ibcs2_msgsnd(td, uap));
	default:
		return (EINVAL);
	}
}

/*
 * iBCS2 semsys call
 */
#ifdef unused
static void
cvt_sem2isem(bp, ibp)
struct sem *bp;
struct ibcs2_sem *ibp;
{
	ibp->semval = bp->semval;
	ibp->sempid = bp->sempid;
	ibp->semncnt = bp->semncnt;
	ibp->semzcnt = bp->semzcnt;
	return;
}

static void
cvt_isem2sem(ibp, bp)
struct ibcs2_sem *ibp;
struct sem *bp;
{
	bp->semval = ibp->semval;
	bp->sempid = ibp->sempid;
	bp->semncnt = ibp->semncnt;
	bp->semzcnt = ibp->semzcnt;
	return;
}
#endif

static void
cvt_iperm2perm(ipp, pp)
struct ibcs2_ipc_perm *ipp;
struct ipc_perm *pp;
{
	pp->uid = ipp->uid;
	pp->gid = ipp->gid;
	pp->cuid = ipp->cuid;
	pp->cgid = ipp->cgid;
	pp->mode = ipp->mode;
	pp->seq = ipp->seq;
	pp->key = ipp->key;
}

static void
cvt_perm2iperm(pp, ipp)
struct ipc_perm *pp;
struct ibcs2_ipc_perm *ipp;
{
	ipp->uid = pp->uid;
	ipp->gid = pp->gid;
	ipp->cuid = pp->cuid;
	ipp->cgid = pp->cgid;
	ipp->mode = pp->mode;
	ipp->seq = pp->seq;
	ipp->key = pp->key;
}

static void
cvt_semid2isemid(bp, ibp)
struct semid_ds *bp;
struct ibcs2_semid_ds *ibp;
{
	cvt_perm2iperm(&bp->sem_perm, &ibp->sem_perm);
	ibp->sem_base = (struct ibcs2_sem *)bp->sem_base;
	ibp->sem_nsems = bp->sem_nsems;
	ibp->sem_otime = bp->sem_otime;
	ibp->sem_ctime = bp->sem_ctime;
	return;
}

static void
cvt_isemid2semid(ibp, bp)
struct ibcs2_semid_ds *ibp;
struct semid_ds *bp;
{
	cvt_iperm2perm(&ibp->sem_perm, &bp->sem_perm);
	bp->sem_base = (struct sem *)ibp->sem_base;
	bp->sem_nsems = ibp->sem_nsems;
	bp->sem_otime = ibp->sem_otime;
	bp->sem_ctime = ibp->sem_ctime;
	return;
}

struct ibcs2_semctl_args {
	int what;
	int semid;
	int semnum;
	int cmd;
	union semun arg;
};

static int
ibcs2_semctl(struct thread *td, void *v)
{
	struct ibcs2_semctl_args *uap = v;
	struct ibcs2_semid_ds is;
	struct semid_ds bs;
	union semun semun;
	register_t rval;
	int error;

	switch(uap->cmd) {
	case IBCS2_IPC_STAT:
		semun.buf = &bs;
		error = kern_semctl(td, uap->semid, uap->semnum, IPC_STAT,
		    &semun, &rval);
		if (error)
			return (error);
		cvt_semid2isemid(&bs, &is);
		error = copyout(&is, uap->arg.buf, sizeof(is));
		if (error == 0)
			td->td_retval[0] = rval;
		return (error);

	case IBCS2_IPC_SET:
		error = copyin(uap->arg.buf, &is, sizeof(is));
		if (error)
			return (error);
		cvt_isemid2semid(&is, &bs);
		semun.buf = &bs;
		return (kern_semctl(td, uap->semid, uap->semnum, IPC_SET,
		    &semun, td->td_retval));
	}

	return (kern_semctl(td, uap->semid, uap->semnum, uap->cmd, &uap->arg,
	    td->td_retval));
}

struct ibcs2_semget_args {
	int what;
	ibcs2_key_t key;
	int nsems;
	int semflg;
};

static int
ibcs2_semget(struct thread *td, void *v)
{
	struct ibcs2_semget_args *uap = v;
	struct semget_args ap;

	ap.key = uap->key;
	ap.nsems = uap->nsems;
	ap.semflg = uap->semflg;
	return (sys_semget(td, &ap));
}

struct ibcs2_semop_args {
	int what;
	int semid;
	struct sembuf *sops;
	size_t nsops;
};

static int
ibcs2_semop(struct thread *td, void *v)
{
	struct ibcs2_semop_args *uap = v;
	struct semop_args ap;

	ap.semid = uap->semid;
	ap.sops = uap->sops;
	ap.nsops = uap->nsops;
	return (sys_semop(td, &ap));
}

int
ibcs2_semsys(td, uap)
	struct thread *td;
	struct ibcs2_semsys_args *uap;
{

	switch (uap->which) {
	case 0:
		return (ibcs2_semctl(td, uap));
	case 1:
		return (ibcs2_semget(td, uap));
	case 2:
		return (ibcs2_semop(td, uap));
	}
	return (EINVAL);
}


/*
 * iBCS2 shmsys call
 */

static void
cvt_shmid2ishmid(bp, ibp)
struct shmid_ds *bp;
struct ibcs2_shmid_ds *ibp;
{
	cvt_perm2iperm(&bp->shm_perm, &ibp->shm_perm);
	ibp->shm_segsz = bp->shm_segsz;
	ibp->shm_lpid = bp->shm_lpid;
	ibp->shm_cpid = bp->shm_cpid;
	if (bp->shm_nattch > SHRT_MAX)
		ibp->shm_nattch = SHRT_MAX;
	else
		ibp->shm_nattch = bp->shm_nattch;
	ibp->shm_cnattch = 0;			/* ignored anyway */
	ibp->shm_atime = bp->shm_atime;
	ibp->shm_dtime = bp->shm_dtime;
	ibp->shm_ctime = bp->shm_ctime;
	return;
}

static void
cvt_ishmid2shmid(ibp, bp)
struct ibcs2_shmid_ds *ibp;
struct shmid_ds *bp;
{
	cvt_iperm2perm(&ibp->shm_perm, &bp->shm_perm);
	bp->shm_segsz = ibp->shm_segsz;
	bp->shm_lpid = ibp->shm_lpid;
	bp->shm_cpid = ibp->shm_cpid;
	bp->shm_nattch = ibp->shm_nattch;
	bp->shm_atime = ibp->shm_atime;
	bp->shm_dtime = ibp->shm_dtime;
	bp->shm_ctime = ibp->shm_ctime;
	return;
}

struct ibcs2_shmat_args {
	int what;
	int shmid;
	const void *shmaddr;
	int shmflg;
};

static int
ibcs2_shmat(struct thread *td, void *v)
{
	struct ibcs2_shmat_args *uap = v;
	struct shmat_args ap;

	ap.shmid = uap->shmid;
	ap.shmaddr = uap->shmaddr;
	ap.shmflg = uap->shmflg;
	return (sys_shmat(td, &ap));
}

struct ibcs2_shmctl_args {
	int what;
	int shmid;
	int cmd;
	struct ibcs2_shmid_ds *buf;
};

static int
ibcs2_shmctl(struct thread *td, void *v)
{
	struct ibcs2_shmctl_args *uap = v;
	struct ibcs2_shmid_ds is;
	struct shmid_ds bs;
	int error;

	switch(uap->cmd) {
	case IBCS2_IPC_STAT:
		error = kern_shmctl(td, uap->shmid, IPC_STAT, &bs, NULL);
		if (error)
			return (error);
		cvt_shmid2ishmid(&bs, &is);
		return (copyout(&is, uap->buf, sizeof(is)));

	case IBCS2_IPC_SET:
		error = copyin(uap->buf, &is, sizeof(is));
		if (error)
			return (error);
		cvt_ishmid2shmid(&is, &bs);
		return (kern_shmctl(td, uap->shmid, IPC_SET, &bs, NULL));

	case IPC_INFO:
	case SHM_INFO:
	case SHM_STAT:
		/* XXX: */
		return (EINVAL);
	}

	return (kern_shmctl(td, uap->shmid, uap->cmd, NULL, NULL));
}

struct ibcs2_shmdt_args {
	int what;
	const void *shmaddr;
};

static int
ibcs2_shmdt(struct thread *td, void *v)
{
	struct ibcs2_shmdt_args *uap = v;
	struct shmdt_args ap;

	ap.shmaddr = uap->shmaddr;
	return (sys_shmdt(td, &ap));
}

struct ibcs2_shmget_args {
	int what;
	ibcs2_key_t key;
	size_t size;
	int shmflg;
};

static int
ibcs2_shmget(struct thread *td, void *v)
{
	struct ibcs2_shmget_args *uap = v;
	struct shmget_args ap;

	ap.key = uap->key;
	ap.size = uap->size;
	ap.shmflg = uap->shmflg;
	return (sys_shmget(td, &ap));
}

int
ibcs2_shmsys(td, uap)
	struct thread *td;
	struct ibcs2_shmsys_args *uap;
{

	switch (uap->which) {
	case 0:
		return (ibcs2_shmat(td, uap));
	case 1:
		return (ibcs2_shmctl(td, uap));
	case 2:
		return (ibcs2_shmdt(td, uap));
	case 3:
		return (ibcs2_shmget(td, uap));
	}
	return (EINVAL);
}

MODULE_DEPEND(ibcs2, sysvmsg, 1, 1, 1);
MODULE_DEPEND(ibcs2, sysvsem, 1, 1, 1);
MODULE_DEPEND(ibcs2, sysvshm, 1, 1, 1);

/*
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
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
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

int
ibcs2_msgsys(td, uap)
	struct thread *td;
	struct ibcs2_msgsys_args *uap;
{
	switch (uap->which) {
	case 0:				/* msgget */
		uap->which = 1;
		return msgsys(td, (struct msgsys_args *)uap);
	case 1: {			/* msgctl */
		int error;
		struct msgsys_args margs;
		caddr_t sg = stackgap_init();

		margs.which = 0;
		margs.a2 = uap->a2;
		margs.a4 =
		    (int)stackgap_alloc(&sg, sizeof(struct msqid_ds));
		margs.a3 = uap->a3;
		switch (margs.a3) {
		case IBCS2_IPC_STAT:
			error = msgsys(td, &margs);
			if (!error)
				cvt_msqid2imsqid(
				    (struct msqid_ds *)margs.a4,
				    (struct ibcs2_msqid_ds *)uap->a4);
			return error;
		case IBCS2_IPC_SET:
			cvt_imsqid2msqid((struct ibcs2_msqid_ds *)uap->a4,
					 (struct msqid_ds *)margs.a4);
			return msgsys(td, &margs);
		case IBCS2_IPC_RMID:
			return msgsys(td, &margs);
		}
		return EINVAL;
	}
	case 2:				/* msgrcv */
		uap->which = 3;
		return msgsys(td, (struct msgsys_args *)uap);
	case 3:				/* msgsnd */
		uap->which = 2;
		return msgsys(td, (struct msgsys_args *)uap);
	default:
		return EINVAL;
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

int
ibcs2_semsys(td, uap)
	struct thread *td;
	struct ibcs2_semsys_args *uap;
{
	int error;

	switch (uap->which) {
	case 0:					/* semctl */
		switch(uap->a4) {
		case IBCS2_IPC_STAT:
		    {
			struct ibcs2_semid_ds *isp;
			struct semid_ds *sp;
			union semun *sup, ssu;
			caddr_t sg = stackgap_init();


			ssu = (union semun) uap->a5;
			sp = stackgap_alloc(&sg, sizeof(struct semid_ds));
			sup = stackgap_alloc(&sg, sizeof(union semun));
			sup->buf = sp;
			uap->a5 = (int)sup;
			error = semsys(td, (struct semsys_args *)uap);
			if (!error) {
				uap->a5 = (int)ssu.buf;
				isp = stackgap_alloc(&sg, sizeof(*isp));
				cvt_semid2isemid(sp, isp);
				error = copyout((caddr_t)isp,
						(caddr_t)ssu.buf,
						sizeof(*isp));
			}
			return error;
		    }
		case IBCS2_IPC_SET:
		    {
			struct ibcs2_semid_ds *isp;
			struct semid_ds *sp;
			caddr_t sg = stackgap_init();

			isp = stackgap_alloc(&sg, sizeof(*isp));
			sp = stackgap_alloc(&sg, sizeof(*sp));
			error = copyin((caddr_t)uap->a5, (caddr_t)isp,
				       sizeof(*isp));
			if (error)
				return error;
			cvt_isemid2semid(isp, sp);
			uap->a5 = (int)sp;
			return semsys(td, (struct semsys_args *)uap);
		    }
		case IBCS2_SETVAL:
		    {
			union semun *sp;
			caddr_t sg = stackgap_init();

			sp = stackgap_alloc(&sg, sizeof(*sp));
			sp->val = (int) uap->a5;
			uap->a5 = (int)sp;
			return semsys(td, (struct semsys_args *)uap);
		    }
		}

		return semsys(td, (struct semsys_args *)uap);

	case 1:				/* semget */
		return semsys(td, (struct semsys_args *)uap);

	case 2:				/* semop */
		return semsys(td, (struct semsys_args *)uap);
	}
	return EINVAL;
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
	bp->shm_internal = (void *)0;		/* ignored anyway */
	return;
}

int
ibcs2_shmsys(td, uap)
	struct thread *td;
	struct ibcs2_shmsys_args *uap;
{
	int error;

	switch (uap->which) {
	case 0:						/* shmat */
		return shmsys(td, (struct shmsys_args *)uap);

	case 1:						/* shmctl */
		switch(uap->a3) {
		case IBCS2_IPC_STAT:
		    {
			struct ibcs2_shmid_ds *isp;
			struct shmid_ds *sp;
			caddr_t sg = stackgap_init();

			isp = (struct ibcs2_shmid_ds *)uap->a4;
			sp = stackgap_alloc(&sg, sizeof(*sp));
			uap->a4 = (int)sp;
			error = shmsys(td, (struct shmsys_args *)uap);
			if (!error) {
				uap->a4 = (int)isp;
				isp = stackgap_alloc(&sg, sizeof(*isp));
				cvt_shmid2ishmid(sp, isp);
				error = copyout((caddr_t)isp,
						(caddr_t)uap->a4,
						sizeof(*isp));
			}
			return error;
		    }
		case IBCS2_IPC_SET:
		    {
			struct ibcs2_shmid_ds *isp;
			struct shmid_ds *sp;
			caddr_t sg = stackgap_init();

			isp = stackgap_alloc(&sg, sizeof(*isp));
			sp = stackgap_alloc(&sg, sizeof(*sp));
			error = copyin((caddr_t)uap->a4, (caddr_t)isp,
				       sizeof(*isp));
			if (error)
				return error;
			cvt_ishmid2shmid(isp, sp);
			uap->a4 = (int)sp;
			return shmsys(td, (struct shmsys_args *)uap);
		    }
		}

		return shmsys(td, (struct shmsys_args *)uap);

	case 2:						/* shmdt */
		return shmsys(td, (struct shmsys_args *)uap);

	case 3:						/* shmget */
		return shmsys(td, (struct shmsys_args *)uap);
	}
	return EINVAL;
}

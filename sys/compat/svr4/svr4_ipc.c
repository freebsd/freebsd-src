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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
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
static int svr4_setsemun(caddr_t *sgp, union semun **argp,
			      union semun *usp);
static int svr4_semop(struct proc *, void *, register_t *);
static int svr4_semget(struct proc *, void *, register_t *);
static int svr4_semctl(struct proc *, void *, register_t *);
#endif

#ifdef SYSVMSG
static void bsd_to_svr4_msqid_ds(const struct msqid_ds *,
				      struct svr4_msqid_ds *);
static void svr4_to_bsd_msqid_ds(const struct svr4_msqid_ds *,
				      struct msqid_ds *);
static int svr4_msgsnd(struct proc *, void *, register_t *);
static int svr4_msgrcv(struct proc *, void *, register_t *);
static int svr4_msgget(struct proc *, void *, register_t *);
static int svr4_msgctl(struct proc *, void *, register_t *);
#endif

#ifdef SYSVSHM
static void bsd_to_svr4_shmid_ds(const struct shmid_ds *,
				      struct svr4_shmid_ds *);
static void svr4_to_bsd_shmid_ds(const struct svr4_shmid_ds *,
				      struct shmid_ds *);
static int svr4_shmat(struct proc *, void *, register_t *);
static int svr4_shmdt(struct proc *, void *, register_t *);
static int svr4_shmget(struct proc *, void *, register_t *);
static int svr4_shmctl(struct proc *, void *, register_t *);
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
	bsd_to_svr4_ipc_perm(&bds->sem_perm, &sds->sem_perm);
	sds->sem_base = (struct svr4_sem *) bds->sem_base;
	sds->sem_nsems = bds->sem_nsems;
	sds->sem_otime = bds->sem_otime;
	sds->sem_pad1 = bds->sem_pad1;
	sds->sem_ctime = bds->sem_ctime;
	sds->sem_pad2 = bds->sem_pad2;
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
	bds->sem_pad1 = sds->sem_pad1;
	bds->sem_ctime = sds->sem_ctime;
	bds->sem_pad2 = sds->sem_pad2;
}

static int
svr4_setsemun(sgp, argp, usp)
	caddr_t *sgp;
	union semun **argp;
	union semun *usp;
{
	*argp = stackgap_alloc(sgp, sizeof(union semun));
	return copyout((caddr_t)usp, *argp, sizeof(union semun));
}

struct svr4_sys_semctl_args {
	syscallarg(int) what;
	syscallarg(int) semid;
	syscallarg(int) semnum;
	syscallarg(int) cmd;
	syscallarg(union semun) arg;
};

static int
svr4_semctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	int error;
	struct svr4_sys_semctl_args *uap = v;
	struct sys___semctl_args ap;
	struct svr4_semid_ds ss;
	struct semid_ds bs, *bsp;
	caddr_t sg = stackgap_init();

	ap.semid = uap->semid;
	ap.semnum = uap->semnum;

	switch (uap->cmd) {
	case SVR4_SEM_GETZCNT:
	case SVR4_SEM_GETNCNT:
	case SVR4_SEM_GETPID:
	case SVR4_SEM_GETVAL:
		switch (uap->cmd) {
		case SVR4_SEM_GETZCNT:
			ap.cmd = GETZCNT;
			break;
		case SVR4_SEM_GETNCNT:
			ap.cmd = GETNCNT;
			break;
		case SVR4_SEM_GETPID:
			ap.cmd = GETPID;
			break;
		case SVR4_SEM_GETVAL:
			ap.cmd = GETVAL;
			break;
		}
		return sys___semctl(p, &ap, retval);

	case SVR4_SEM_SETVAL:
		error = svr4_setsemun(&sg, &ap.arg, &uap->arg);
		if (error)
			return error;
		ap.cmd = SETVAL;
		return sys___semctl(p, &ap, retval);

	case SVR4_SEM_GETALL:
		error = svr4_setsemun(&sg, &ap.arg, &uap->arg);
		if (error)
			return error;
		ap.cmd = GETVAL;
		return sys___semctl(p, &ap, retval);

	case SVR4_SEM_SETALL:
		error = svr4_setsemun(&sg, &ap.arg, &uap->arg);
		if (error)
			return error;
		ap.cmd = SETVAL;
		return sys___semctl(p, &ap, retval);

	case SVR4_IPC_STAT:
                ap.cmd = IPC_STAT;
		bsp = stackgap_alloc(&sg, sizeof(bs));
		error = svr4_setsemun(&sg, &ap.arg,
				      (union semun *)&bsp);
		if (error)
			return error;
                if ((error = sys___semctl(p, &ap, retval)) != 0)
                        return error;
		error = copyin((caddr_t)bsp, (caddr_t)&bs, sizeof(bs));
                if (error)
                        return error;
                bsd_to_svr4_semid_ds(&bs, &ss);
		return copyout(&ss, uap->arg.buf, sizeof(ss));

	case SVR4_IPC_SET:
		ap.cmd = IPC_SET;
		bsp = stackgap_alloc(&sg, sizeof(bs));
		error = svr4_setsemun(&sg, &ap.arg,
				      (union semun *)&bsp);
		if (error)
			return error;
		error = copyin(uap->arg.buf, (caddr_t) &ss, sizeof ss);
                if (error)
                        return error;
                svr4_to_bsd_semid_ds(&ss, &bs);
		error = copyout(&bs, bsp, sizeof(bs));
                if (error)
                        return error;
		return sys___semctl(p, &ap, retval);

	case SVR4_IPC_RMID:
		ap.cmd = IPC_RMID;
		bsp = stackgap_alloc(&sg, sizeof(bs));
		error = svr4_setsemun(&sg, &ap.arg,
				      (union semun *)&bsp);
		if (error)
			return error;
		error = copyin(uap->arg.buf, &ss, sizeof ss);
                if (error)
                        return error;
                svr4_to_bsd_semid_ds(&ss, &bs);
		error = copyout(&bs, bsp, sizeof(bs));
		if (error)
			return error;
		return sys___semctl(p, &ap, retval);

	default:
		return EINVAL;
	}
}

struct svr4_sys_semget_args {
	syscallarg(int) what;
	syscallarg(svr4_key_t) key;
	syscallarg(int) nsems;
	syscallarg(int) semflg;
};

static int
svr4_semget(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_semget_args *uap = v;
	struct sys_semget_args ap;

	ap.key = uap->key;
	ap.nsems = uap->nsems;
	ap.semflg = uap->semflg;

	return sys_semget(p, &ap, retval);
}

struct svr4_sys_semop_args {
	syscallarg(int) what;
	syscallarg(int) semid;
	syscallarg(struct svr4_sembuf *) sops;
	syscallarg(u_int) nsops;
};

static int
svr4_semop(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_semop_args *uap = v;
	struct sys_semop_args ap;

	ap.semid = uap->semid;
	/* These are the same */
	ap.sops = (struct sembuf *) uap->sops;
	ap.nsops = uap->nsops;

	return sys_semop(p, &ap, retval);
}

int
svr4_sys_semsys(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_semsys_args *uap = v;

	DPRINTF(("svr4_semsys(%d)\n", uap->what));

	switch (uap->what) {
	case SVR4_semctl:
		return svr4_semctl(p, v, retval);
	case SVR4_semget:
		return svr4_semget(p, v, retval);
	case SVR4_semop:
		return svr4_semop(p, v, retval);
	default:
		return EINVAL;
	}
}
#endif

#ifdef SYSVMSG
static void
bsd_to_svr4_msqid_ds(bds, sds)
	const struct msqid_ds *bds;
	struct svr4_msqid_ds *sds;
{
	bsd_to_svr4_ipc_perm(&bds->msg_perm, &sds->msg_perm);
	sds->msg_first = (struct svr4_msg *) bds->msg_first;
	sds->msg_last = (struct svr4_msg *) bds->msg_last;
	sds->msg_cbytes = bds->msg_cbytes;
	sds->msg_qnum = bds->msg_qnum;
	sds->msg_qbytes = bds->msg_qbytes;
	sds->msg_lspid = bds->msg_lspid;
	sds->msg_lrpid = bds->msg_lrpid;
	sds->msg_stime = bds->msg_stime;
	sds->msg_pad1 = bds->msg_pad1;
	sds->msg_rtime = bds->msg_rtime;
	sds->msg_pad2 = bds->msg_pad2;
	sds->msg_ctime = bds->msg_ctime;
	sds->msg_pad3 = bds->msg_pad3;

	/* use the padding for the rest of the fields */
	{
		const short *pad = (const short *) bds->msg_pad4;
		sds->msg_cv = pad[0];
		sds->msg_qnum_cv = pad[1];
	}
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
	bds->msg_pad1 = sds->msg_pad1;
	bds->msg_rtime = sds->msg_rtime;
	bds->msg_pad2 = sds->msg_pad2;
	bds->msg_ctime = sds->msg_ctime;
	bds->msg_pad3 = sds->msg_pad3;

	/* use the padding for the rest of the fields */
	{
		short *pad = (short *) bds->msg_pad4;
		pad[0] = sds->msg_cv;
		pad[1] = sds->msg_qnum_cv;
	}
}

struct svr4_sys_msgsnd_args {
	syscallarg(int) what;
	syscallarg(int) msqid;
	syscallarg(void *) msgp;
	syscallarg(size_t) msgsz;
	syscallarg(int) msgflg;
};

static int
svr4_msgsnd(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_msgsnd_args *uap = v;
	struct sys_msgsnd_args ap;

	ap.msqid = uap->msqid;
	ap.msgp = uap->msgp;
	ap.msgsz = uap->msgsz;
	ap.msgflg = uap->msgflg;

	return sys_msgsnd(p, &ap, retval);
}

struct svr4_sys_msgrcv_args {
	syscallarg(int) what;
	syscallarg(int) msqid;
	syscallarg(void *) msgp;
	syscallarg(size_t) msgsz;
	syscallarg(long) msgtyp;
	syscallarg(int) msgflg;
};

static int
svr4_msgrcv(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_msgrcv_args *uap = v;
	struct sys_msgrcv_args ap;

	ap.msqid = uap->msqid;
	ap.msgp = uap->msgp;
	ap.msgsz = uap->msgsz;
	ap.msgtyp = uap->msgtyp;
	ap.msgflg = uap->msgflg;

	return sys_msgrcv(p, &ap, retval);
}
	
struct svr4_sys_msgget_args {
	syscallarg(int) what;
	syscallarg(svr4_key_t) key;
	syscallarg(int) msgflg;
};

static int
svr4_msgget(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_msgget_args *uap = v;
	struct sys_msgget_args ap;

	ap.key = uap->key;
	ap.msgflg = uap->msgflg;

	return sys_msgget(p, &ap, retval);
}

struct svr4_sys_msgctl_args {
	syscallarg(int) what;
	syscallarg(int) msqid;
	syscallarg(int) cmd;
	syscallarg(struct svr4_msqid_ds *) buf;
};

static int
svr4_msgctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
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
		error = copyin(uap->buf, &ss, sizeof ss);
		if (error)
			return error;
		svr4_to_bsd_msqid_ds(&ss, &bs);
		return (kern_msgctl(td, uap->msqid, IPC_RMID, &bs));

	default:
		return EINVAL;
	}
}

int
svr4_sys_msgsys(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_msgsys_args *uap = v;

	DPRINTF(("svr4_msgsys(%d)\n", uap->what));

	switch (uap->what) {
	case SVR4_msgsnd:
		return svr4_msgsnd(p, v, retval);
	case SVR4_msgrcv:
		return svr4_msgrcv(p, v, retval);
	case SVR4_msgget:
		return svr4_msgget(p, v, retval);
	case SVR4_msgctl:
		return svr4_msgctl(p, v, retval);
	default:
		return EINVAL;
	}
}
#endif

#ifdef SYSVSHM

static void
bsd_to_svr4_shmid_ds(bds, sds)
	const struct shmid_ds *bds;
	struct svr4_shmid_ds *sds;
{
	bsd_to_svr4_ipc_perm(&bds->shm_perm, &sds->shm_perm);
	sds->shm_segsz = bds->shm_segsz;
	sds->shm_lkcnt = 0;
	sds->shm_lpid = bds->shm_lpid;
	sds->shm_cpid = bds->shm_cpid;
	sds->shm_amp = bds->shm_internal;
	sds->shm_nattch = bds->shm_nattch;
	sds->shm_cnattch = 0;
	sds->shm_atime = bds->shm_atime;
	sds->shm_pad1 = 0;
	sds->shm_dtime = bds->shm_dtime;
	sds->shm_pad2 = 0;
	sds->shm_ctime = bds->shm_ctime;
	sds->shm_pad3 = 0;
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
	bds->shm_internal = sds->shm_amp;
	bds->shm_nattch = sds->shm_nattch;
	bds->shm_atime = sds->shm_atime;
	bds->shm_dtime = sds->shm_dtime;
	bds->shm_ctime = sds->shm_ctime;
}

struct svr4_sys_shmat_args {
	syscallarg(int) what;
	syscallarg(int) shmid;
	syscallarg(void *) shmaddr;
	syscallarg(int) shmflg;
};

static int
svr4_shmat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_shmat_args *uap = v;
	struct sys_shmat_args ap;

	ap.shmid = uap->shmid;
	ap.shmaddr = uap->shmaddr;
	ap.shmflg = uap->shmflg;

	return sys_shmat(p, &ap, retval);
}

struct svr4_sys_shmdt_args {
	syscallarg(int) what;
	syscallarg(void *) shmaddr;
};

static int
svr4_shmdt(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_shmdt_args *uap = v;
	struct sys_shmdt_args ap;

	ap.shmaddr = uap->shmaddr;

	return sys_shmdt(p, &ap, retval);
}

struct svr4_sys_shmget_args {
	syscallarg(int) what;
	syscallarg(key_t) key;
	syscallarg(int) size;
	syscallarg(int) shmflg;
};

static int
svr4_shmget(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_shmget_args *uap = v;
	struct sys_shmget_args ap;

	ap.key = uap->key;
	ap.size = uap->size;
	ap.shmflg = uap->shmflg;

	return sys_shmget(p, &ap, retval);
}

struct svr4_sys_shmctl_args {
	syscallarg(int) what;
	syscallarg(int) shmid;
	syscallarg(int) cmd;
	syscallarg(struct svr4_shmid_ds *) buf;
};

int
svr4_shmctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_shmctl_args *uap = v;
	int error;
	caddr_t sg = stackgap_init();
	struct sys_shmctl_args ap;
	struct shmid_ds bs;
	struct svr4_shmid_ds ss;

	ap.shmid = uap->shmid;

	if (uap->buf != NULL) {
		ap.buf = stackgap_alloc(&sg, sizeof (struct shmid_ds));
		switch (uap->cmd) {
		case SVR4_IPC_SET:
		case SVR4_IPC_RMID:
		case SVR4_SHM_LOCK:
		case SVR4_SHM_UNLOCK:
			error = copyin(uap->buf, (caddr_t) &ss,
			    sizeof ss);
			if (error)
				return error;
			svr4_to_bsd_shmid_ds(&ss, &bs);
			error = copyout(&bs, ap.buf, sizeof bs);
			if (error)
				return error;
			break;
		default:
			break;
		}
	}
	else
		ap.buf = NULL;


	switch (uap->cmd) {
	case SVR4_IPC_STAT:
		ap.cmd = IPC_STAT;
		if ((error = sys_shmctl(p, &ap, retval)) != 0)
			return error;
		if (uap->buf == NULL)
			return 0;
		error = copyin(&bs, ap.buf, sizeof bs);
		if (error)
			return error;
		bsd_to_svr4_shmid_ds(&bs, &ss);
		return copyout(&ss, uap->buf, sizeof ss);

	case SVR4_IPC_SET:
		ap.cmd = IPC_SET;
		return sys_shmctl(p, &ap, retval);

	case SVR4_IPC_RMID:
	case SVR4_SHM_LOCK:
	case SVR4_SHM_UNLOCK:
		switch (uap->cmd) {
		case SVR4_IPC_RMID:
			ap.cmd = IPC_RMID;
			break;
		case SVR4_SHM_LOCK:
			ap.cmd = SHM_LOCK;
			break;
		case SVR4_SHM_UNLOCK:
			ap.cmd = SHM_UNLOCK;
			break;
		default:
			return EINVAL;
		}
		return sys_shmctl(p, &ap, retval);

	default:
		return EINVAL;
	}
}

int
svr4_sys_shmsys(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_shmsys_args *uap = v;

	DPRINTF(("svr4_shmsys(%d)\n", uap->what));

	switch (uap->what) {
	case SVR4_shmat:
		return svr4_shmat(p, v, retval);
	case SVR4_shmdt:
		return svr4_shmdt(p, v, retval);
	case SVR4_shmget:
		return svr4_shmget(p, v, retval);
	case SVR4_shmctl:
		return svr4_shmctl(p, v, retval);
	default:
		return ENOSYS;
	}
}
#endif /* SYSVSHM */

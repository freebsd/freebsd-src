/*-
 * Copyright (c) 1994-1995 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
 * $FreeBSD: src/sys/i386/linux/linux_ipc.c,v 1.16 1999/09/23 09:57:45 marcel Exp $
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/proc.h>
#include <sys/sem.h>
#include <sys/shm.h>

#include <i386/linux/linux.h>
#include <i386/linux/linux_proto.h>
#include <i386/linux/linux_util.h>

static int linux_semop __P((struct proc *, struct linux_ipc_args *));
static int linux_semget __P((struct proc *, struct linux_ipc_args *));
static int linux_semctl __P((struct proc *, struct linux_ipc_args *));
static int linux_msgsnd __P((struct proc *, struct linux_ipc_args *));
static int linux_msgrcv __P((struct proc *, struct linux_ipc_args *));
static int linux_msgctl __P((struct proc *, struct linux_ipc_args *));
static int linux_shmat __P((struct proc *, struct linux_ipc_args *));
static int linux_shmdt __P((struct proc *, struct linux_ipc_args *));
static int linux_shmget __P((struct proc *, struct linux_ipc_args *));
static int linux_shmctl __P((struct proc *, struct linux_ipc_args *));

struct linux_ipc_perm {
    linux_key_t key;
    unsigned short uid;
    unsigned short gid;
    unsigned short cuid;
    unsigned short cgid;
    unsigned short mode;
    unsigned short seq;
};

static void
linux_to_bsd_ipc_perm(struct linux_ipc_perm *lpp, struct ipc_perm *bpp)
{
    bpp->key = lpp->key;
    bpp->uid = lpp->uid;
    bpp->gid = lpp->gid;
    bpp->cuid = lpp->cuid;
    bpp->cgid = lpp->cgid;
    bpp->mode = lpp->mode;
    bpp->seq = lpp->seq;
}


static void
bsd_to_linux_ipc_perm(struct ipc_perm *bpp, struct linux_ipc_perm *lpp)
{
    lpp->key = bpp->key;
    lpp->uid = bpp->uid;
    lpp->gid = bpp->gid;
    lpp->cuid = bpp->cuid;
    lpp->cgid = bpp->cgid;
    lpp->mode = bpp->mode;
    lpp->seq = bpp->seq;
}

struct linux_semid_ds {
	struct linux_ipc_perm	sem_perm;
	linux_time_t		sem_otime;
	linux_time_t		sem_ctime;
	void			*sem_base;
	void			*sem_pending;
	void			*sem_pending_last;
	void			*undo;
	ushort			sem_nsems;
};

struct linux_shmid_ds {
    struct linux_ipc_perm shm_perm;
    int shm_segsz;
    linux_time_t shm_atime;
    linux_time_t shm_dtime;
    linux_time_t shm_ctime;
    ushort shm_cpid;
    ushort shm_lpid;
    short shm_nattch;
    ushort private1;
    void *private2;
    void *private3;
};

static void
linux_to_bsd_semid_ds(struct linux_semid_ds *lsp, struct semid_ds *bsp)
{
    linux_to_bsd_ipc_perm(&lsp->sem_perm, &bsp->sem_perm);
    bsp->sem_otime = lsp->sem_otime;
    bsp->sem_ctime = lsp->sem_ctime;
    bsp->sem_nsems = lsp->sem_nsems;
    bsp->sem_base = lsp->sem_base;
}

static void
bsd_to_linux_semid_ds(struct semid_ds *bsp, struct linux_semid_ds *lsp)
{
	bsd_to_linux_ipc_perm(&bsp->sem_perm, &lsp->sem_perm);
	lsp->sem_otime = bsp->sem_otime;
	lsp->sem_ctime = bsp->sem_ctime;
	lsp->sem_nsems = bsp->sem_nsems;
	lsp->sem_base = bsp->sem_base;
}

static void
linux_to_bsd_shmid_ds(struct linux_shmid_ds *lsp, struct shmid_ds *bsp)
{
    linux_to_bsd_ipc_perm(&lsp->shm_perm, &bsp->shm_perm);
    bsp->shm_segsz = lsp->shm_segsz;
    bsp->shm_lpid = lsp->shm_lpid;
    bsp->shm_cpid = lsp->shm_cpid;
    bsp->shm_nattch = lsp->shm_nattch;
    bsp->shm_atime = lsp->shm_atime;
    bsp->shm_dtime = lsp->shm_dtime;
    bsp->shm_ctime = lsp->shm_ctime;
    bsp->shm_internal = lsp->private3;	/* this goes (yet) SOS */
}

static void
bsd_to_linux_shmid_ds(struct shmid_ds *bsp, struct linux_shmid_ds *lsp)
{
    bsd_to_linux_ipc_perm(&bsp->shm_perm, &lsp->shm_perm);
    lsp->shm_segsz = bsp->shm_segsz;
    lsp->shm_lpid = bsp->shm_lpid;
    lsp->shm_cpid = bsp->shm_cpid;
    lsp->shm_nattch = bsp->shm_nattch;
    lsp->shm_atime = bsp->shm_atime;
    lsp->shm_dtime = bsp->shm_dtime;
    lsp->shm_ctime = bsp->shm_ctime;
    lsp->private3 = bsp->shm_internal;	/* this goes (yet) SOS */
}

static int
linux_semop(struct proc *p, struct linux_ipc_args *args)
{
	struct semop_args /* {
	int	semid;
	struct	sembuf *sops;
	int		nsops;
	} */ bsd_args;

	bsd_args.semid = args->arg1;
	bsd_args.sops = (struct sembuf *)args->ptr;
	bsd_args.nsops = args->arg2;
	return semop(p, &bsd_args);
}

static int
linux_semget(struct proc *p, struct linux_ipc_args *args)
{
	struct semget_args /* {
	key_t	key;
	int		nsems;
	int		semflg;
	} */ bsd_args;

	bsd_args.key = args->arg1;
	bsd_args.nsems = args->arg2;
	bsd_args.semflg = args->arg3;
	return semget(p, &bsd_args);
}

static int
linux_semctl(struct proc *p, struct linux_ipc_args *args)
{
	struct linux_semid_ds	linux_semid;
	struct semid_ds	bsd_semid;
	struct __semctl_args /* {
	int		semid;
	int		semnum;
	int		cmd;
	union	semun *arg;
	} */ bsd_args;
	int	error;
	caddr_t sg, unptr, dsp, ldsp;

	sg = stackgap_init();
	bsd_args.semid = args->arg1;
	bsd_args.semnum = args->arg2;
	bsd_args.cmd = args->arg3;
	bsd_args.arg = (union semun *)args->ptr;

	switch (args->arg3) {
	case LINUX_IPC_RMID:
		bsd_args.cmd = IPC_RMID;
		break;
	case LINUX_GETNCNT:
		bsd_args.cmd = GETNCNT;
		break;
	case LINUX_GETPID:
		bsd_args.cmd = GETPID;
		break;
	case LINUX_GETVAL:
		bsd_args.cmd = GETVAL;
		break;
	case LINUX_GETZCNT:
		bsd_args.cmd = GETZCNT;
		break;
	case LINUX_SETVAL:
		bsd_args.cmd = SETVAL;
		break;
	case LINUX_IPC_SET:
		bsd_args.cmd = IPC_SET;
		error = copyin(args->ptr, &ldsp, sizeof(ldsp));
		if (error)
			return error;
		error = copyin(ldsp, (caddr_t)&linux_semid, sizeof(linux_semid));
		if (error)
			return error;
		linux_to_bsd_semid_ds(&linux_semid, &bsd_semid);
		unptr = stackgap_alloc(&sg, sizeof(union semun));
		dsp = stackgap_alloc(&sg, sizeof(struct semid_ds));
		error = copyout((caddr_t)&bsd_semid, dsp, sizeof(bsd_semid));
		if (error)
			return error;
		error = copyout((caddr_t)&dsp, unptr, sizeof(dsp));
		if (error)
			return error;
		bsd_args.arg = (union semun *)unptr;
		return __semctl(p, &bsd_args);
	case LINUX_IPC_STAT:
		bsd_args.cmd = IPC_STAT;
		unptr = stackgap_alloc(&sg, sizeof(union semun *));
		dsp = stackgap_alloc(&sg, sizeof(struct semid_ds));
		error = copyout((caddr_t)&dsp, unptr, sizeof(dsp));
		if (error)
			return error;
		bsd_args.arg = (union semun *)unptr;
		error = __semctl(p, &bsd_args);
		if (error)
			return error;
		error = copyin(dsp, (caddr_t)&bsd_semid, sizeof(bsd_semid));
		if (error)
			return error;
		bsd_to_linux_semid_ds(&bsd_semid, &linux_semid);
		error = copyin(args->ptr, &ldsp, sizeof(ldsp));
		if (error)
			return error;
		return copyout((caddr_t)&linux_semid, ldsp, sizeof(linux_semid));
	case LINUX_GETALL:
		/* FALLTHROUGH */
	case LINUX_SETALL:
		/* FALLTHROUGH */
	default:
		uprintf("LINUX: 'ipc' typ=%d not implemented\n", args->what);
		return EINVAL;
	}
	return __semctl(p, &bsd_args);
}

static int
linux_msgsnd(struct proc *p, struct linux_ipc_args *args)
{
    struct msgsnd_args /* {
	int     msqid;   
	void    *msgp;   
	size_t  msgsz;   
	int     msgflg; 
    } */ bsd_args;

    bsd_args.msqid = args->arg1;
    bsd_args.msgp = args->ptr;
    bsd_args.msgsz = args->arg2;
    bsd_args.msgflg = args->arg3;
    return msgsnd(p, &bsd_args);
}

static int
linux_msgrcv(struct proc *p, struct linux_ipc_args *args)
{
    struct msgrcv_args /* {     
        int 	msqid;   
	void	*msgp;   
	size_t	msgsz;   
	long	msgtyp; 
	int	msgflg; 
    } */ bsd_args; 

    bsd_args.msqid = args->arg1;
    bsd_args.msgp = args->ptr;
    bsd_args.msgsz = args->arg2;
    bsd_args.msgtyp = 0;
    bsd_args.msgflg = args->arg3;
    return msgrcv(p, &bsd_args);
}

static int
linux_msgget(struct proc *p, struct linux_ipc_args *args)
{
    struct msgget_args /* {
	key_t	key;
        int 	msgflg;
    } */ bsd_args;

    bsd_args.key = args->arg1;
    bsd_args.msgflg = args->arg2;
    return msgget(p, &bsd_args);
}

static int
linux_msgctl(struct proc *p, struct linux_ipc_args *args)
{
    struct msgctl_args /* {
	int     msqid; 
	int     cmd;
	struct	msqid_ds *buf;
    } */ bsd_args;
    int error;

    bsd_args.msqid = args->arg1;
    bsd_args.cmd = args->arg2;
    bsd_args.buf = (struct msqid_ds *)args->ptr;
    error = msgctl(p, &bsd_args);
    return ((args->arg2 == LINUX_IPC_RMID && error == EINVAL) ? 0 : error);
}

static int
linux_shmat(struct proc *p, struct linux_ipc_args *args)
{
    struct shmat_args /* {
	int shmid;
	void *shmaddr;
	int shmflg;
    } */ bsd_args;
    int error;

    bsd_args.shmid = args->arg1;
    bsd_args.shmaddr = args->ptr;
    bsd_args.shmflg = args->arg2;
    if ((error = shmat(p, &bsd_args)))
	return error;
    if ((error = copyout(p->p_retval, (caddr_t)args->arg3, sizeof(int))))
	return error;
    p->p_retval[0] = 0;
    return 0;
}

static int
linux_shmdt(struct proc *p, struct linux_ipc_args *args)
{
    struct shmdt_args /* {
	void *shmaddr;
    } */ bsd_args;

    bsd_args.shmaddr = args->ptr;
    return shmdt(p, &bsd_args);
}

static int
linux_shmget(struct proc *p, struct linux_ipc_args *args)
{
    struct shmget_args /* {
	key_t key;
	int size;
	int shmflg;
    } */ bsd_args;

    bsd_args.key = args->arg1;
    bsd_args.size = args->arg2;
    bsd_args.shmflg = args->arg3;
    return shmget(p, &bsd_args);
}

static int
linux_shmctl(struct proc *p, struct linux_ipc_args *args)
{
    struct shmid_ds bsd_shmid;
    struct linux_shmid_ds linux_shmid;
    struct shmctl_args /* {
	int shmid;
	int cmd;
	struct shmid_ds *buf;
    } */ bsd_args;
    int error;
    caddr_t sg = stackgap_init();

    switch (args->arg2) {
    case LINUX_IPC_STAT:
	bsd_args.shmid = args->arg1;
	bsd_args.cmd = IPC_STAT;
	bsd_args.buf = (struct shmid_ds*)stackgap_alloc(&sg, sizeof(struct shmid_ds));
	if ((error = shmctl(p, &bsd_args)))
	    return error;
	if ((error = copyin((caddr_t)bsd_args.buf, (caddr_t)&bsd_shmid,
		    	    sizeof(struct shmid_ds))))
	    return error;
	bsd_to_linux_shmid_ds(&bsd_shmid, &linux_shmid);
	return copyout((caddr_t)&linux_shmid, args->ptr, sizeof(linux_shmid));

    case LINUX_IPC_SET:
	if ((error = copyin(args->ptr, (caddr_t)&linux_shmid,
		    	    sizeof(linux_shmid))))
	    return error;
	linux_to_bsd_shmid_ds(&linux_shmid, &bsd_shmid);
	bsd_args.buf = (struct shmid_ds*)stackgap_alloc(&sg, sizeof(struct shmid_ds));
	if ((error = copyout((caddr_t)&bsd_shmid, (caddr_t)bsd_args.buf,
		     	     sizeof(struct shmid_ds))))
	    return error;
	bsd_args.shmid = args->arg1;
	bsd_args.cmd = IPC_SET;
	return shmctl(p, &bsd_args);

    case LINUX_IPC_RMID:
	bsd_args.shmid = args->arg1;
	bsd_args.cmd = IPC_RMID;
	if (NULL == args->ptr)
	    bsd_args.buf = NULL;
	else {
	    if ((error = copyin(args->ptr, (caddr_t)&linux_shmid, 
		    		sizeof(linux_shmid))))
		return error;
	    linux_to_bsd_shmid_ds(&linux_shmid, &bsd_shmid);
	    bsd_args.buf = (struct shmid_ds*)stackgap_alloc(&sg, sizeof(struct shmid_ds));
	    if ((error = copyout((caddr_t)&bsd_shmid, (caddr_t)bsd_args.buf,
		     		 sizeof(struct shmid_ds))))
		return error;
	}
	return shmctl(p, &bsd_args);

    case LINUX_IPC_INFO:
    case LINUX_SHM_STAT:
    case LINUX_SHM_INFO:
    case LINUX_SHM_LOCK:
    case LINUX_SHM_UNLOCK:
    default:
	uprintf("LINUX: 'ipc' typ=%d not implemented\n", args->what);
	return EINVAL;
    }
}

int
linux_ipc(struct proc *p, struct linux_ipc_args *args)
{
    switch (args->what) {
    case LINUX_SEMOP:
	return linux_semop(p, args);
    case LINUX_SEMGET:
	return linux_semget(p, args);
    case LINUX_SEMCTL:
	return linux_semctl(p, args);
    case LINUX_MSGSND:
	return linux_msgsnd(p, args);
    case LINUX_MSGRCV:
	return linux_msgrcv(p, args);
    case LINUX_MSGGET:
	return linux_msgget(p, args);
    case LINUX_MSGCTL:
	return linux_msgctl(p, args);
    case LINUX_SHMAT:
	return linux_shmat(p, args);
    case LINUX_SHMDT:
	return linux_shmdt(p, args);
    case LINUX_SHMGET:
	return linux_shmget(p, args);
    case LINUX_SHMCTL:
	return linux_shmctl(p, args);
    default:
	uprintf("LINUX: 'ipc' typ=%d not implemented\n", args->what);
	return ENOSYS;
    }
}

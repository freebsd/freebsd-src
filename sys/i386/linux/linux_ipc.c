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
 *  $Id: linux_ipc.c,v 1.2 1995/11/22 07:43:47 bde Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/proc.h>
#include <sys/shm.h>

#include <i386/linux/linux.h>
#include <i386/linux/sysproto.h>

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

struct linux_ipc_args {
    int what;
    int arg1;
    int arg2;
    int arg3;
    caddr_t ptr;
};

int
linux_semop(struct proc *p, struct linux_ipc_args *args, int *retval)
{
    return ENOSYS;
}

int
linux_semget(struct proc *p, struct linux_ipc_args *args, int *retval)
{
    return ENOSYS;
}

int
linux_semctl(struct proc *p, struct linux_ipc_args *args, int *retval)
{
    return ENOSYS;
}

int
linux_msgsnd(struct proc *p, struct linux_ipc_args *args, int *retval)
{
    return ENOSYS;
}

int
linux_msgrcv(struct proc *p, struct linux_ipc_args *args, int *retval)
{
    return ENOSYS;
}

int
linux_msgget(struct proc *p, struct linux_ipc_args *args, int *retval)
{
    return ENOSYS;
}

int
linux_msgctl(struct proc *p, struct linux_ipc_args *args, int *retval)
{
    return ENOSYS;
}

int
linux_shmat(struct proc *p, struct linux_ipc_args *args, int *retval)
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
    if ((error = shmat(p, &bsd_args, retval)))
	return error;
    if ((error = copyout(retval, (caddr_t)args->arg3, sizeof(int))))
	return error;
    retval[0] = 0;
    return 0;
}

int
linux_shmdt(struct proc *p, struct linux_ipc_args *args, int *retval)
{
    struct shmdt_args /* {
	void *shmaddr;
    } */ bsd_args;

    bsd_args.shmaddr = args->ptr;
    return shmdt(p, &bsd_args, retval);
}

int
linux_shmget(struct proc *p, struct linux_ipc_args *args, int *retval)
{
    struct shmget_args /* {
	key_t key;
	int size;
	int shmflg;
    } */ bsd_args;

    bsd_args.key = args->arg1;
    bsd_args.size = args->arg2;
    bsd_args.shmflg = args->arg3;
    return shmget(p, &bsd_args, retval);
}

int
linux_shmctl(struct proc *p, struct linux_ipc_args *args, int *retval)
{
    struct shmid_ds bsd_shmid;
    struct linux_shmid_ds linux_shmid;
    struct shmctl_args /* {
	int shmid;
	int cmd;
	struct shmid_ds *buf;
    } */ bsd_args;
    int error;

    switch (args->arg2) {
    case LINUX_IPC_STAT:
	bsd_args.shmid = args->arg1;
	bsd_args.cmd = IPC_STAT;
	bsd_args.buf = (struct shmid_ds*)ua_alloc_init(sizeof(struct shmid_ds));
	if ((error = shmctl(p, &bsd_args, retval)))
	    return error;
	if ((error = copyin((caddr_t)&bsd_shmid, (caddr_t)bsd_args.buf,
		    	    sizeof(struct shmid_ds))))
	    return error;
	bsd_to_linux_shmid_ds(&bsd_shmid, &linux_shmid);
	return copyout((caddr_t)&linux_shmid, args->ptr, sizeof(linux_shmid));

    case LINUX_IPC_SET:
	if ((error = copyin(args->ptr, (caddr_t)&linux_shmid,
		    	    sizeof(linux_shmid))))
	    return error;
	linux_to_bsd_shmid_ds(&linux_shmid, &bsd_shmid);
	bsd_args.buf = (struct shmid_ds*)ua_alloc_init(sizeof(struct shmid_ds));
	if ((error = copyout((caddr_t)&bsd_shmid, (caddr_t)bsd_args.buf,
		     	     sizeof(struct shmid_ds))))
	    return error;
	bsd_args.shmid = args->arg1;
	bsd_args.cmd = IPC_SET;
	return shmctl(p, &bsd_args, retval);

    case LINUX_IPC_RMID:
	bsd_args.shmid = args->arg1;
	bsd_args.cmd = IPC_RMID;
	if ((error = copyin(args->ptr, (caddr_t)&linux_shmid, 
		    	    sizeof(linux_shmid))))
	    return error;
	linux_to_bsd_shmid_ds(&linux_shmid, &bsd_shmid);
	bsd_args.buf = (struct shmid_ds*)ua_alloc_init(sizeof(struct shmid_ds));
	if ((error = copyout((caddr_t)&bsd_shmid, (caddr_t)bsd_args.buf,
		     	     sizeof(struct shmid_ds))))
	    return error;
	return shmctl(p, &bsd_args, retval);

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
linux_ipc(struct proc *p, struct linux_ipc_args *args, int *retval)
{
    switch (args->what) {
    case LINUX_SEMOP:
	return linux_semop(p, args, retval);
    case LINUX_SEMGET:
	return linux_semget(p, args, retval);
    case LINUX_SEMCTL:
	return linux_semctl(p, args, retval);
    case LINUX_MSGSND:
	return linux_msgsnd(p, args, retval);
    case LINUX_MSGRCV:
	return linux_msgrcv(p, args, retval);
    case LINUX_MSGGET:
	return linux_msgget(p, args, retval);
    case LINUX_MSGCTL:
	return linux_msgctl(p, args, retval);
    case LINUX_SHMAT:
	return linux_shmat(p, args, retval);
    case LINUX_SHMDT:
	return linux_shmdt(p, args, retval);
    case LINUX_SHMGET:
	return linux_shmget(p, args, retval);
    case LINUX_SHMCTL:
	return linux_shmctl(p, args, retval);
    default:
	uprintf("LINUX: 'ipc' typ=%d not implemented\n", args->what);
	return ENOSYS;
    }
}

/*	$NetBSD: sysv_shm.c,v 1.23 1994/07/04 23:25:12 glass Exp $	*/
/*-
 * Copyright (c) 1994 Adam Glass and Charles Hannum.  All rights reserved.
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
 *	This product includes software developed by Adam Glass and Charles
 *	Hannum.
 * 4. The names of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*-
 * Copyright (c) 2003-2005 McAfee, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project in part by McAfee
 * Research, the Security Research Division of McAfee, Inc under DARPA/SPAWAR
 * contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA CHATS research
 * program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"
#include "opt_sysvipc.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/sysctl.h>
#include <sys/shm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/resourcevar.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/syscallsubr.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/jail.h>
#include <sys/mac.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_object.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>

#ifdef MAC_DEBUG
#define MPRINTF(a)      printf a
#else
#define MPRINTF(a)	
#endif

static MALLOC_DEFINE(M_SHM, "shm", "SVID compatible shared memory segments");

struct oshmctl_args;
static int oshmctl(struct thread *td, struct oshmctl_args *uap);

static int shmget_allocate_segment(struct thread *td,
    struct shmget_args *uap, int mode);
static int shmget_existing(struct thread *td, struct shmget_args *uap,
    int mode, int segnum);

/* XXX casting to (sy_call_t *) is bogus, as usual. */
static sy_call_t *shmcalls[] = {
	(sy_call_t *)shmat, (sy_call_t *)oshmctl,
	(sy_call_t *)shmdt, (sy_call_t *)shmget,
	(sy_call_t *)shmctl
};

#define	SHMSEG_FREE     	0x0200
#define	SHMSEG_REMOVED  	0x0400
#define	SHMSEG_ALLOCATED	0x0800
#define	SHMSEG_WANTED		0x1000

static int shm_last_free, shm_nused, shm_committed, shmalloced;
static struct shmid_kernel	*shmsegs;

struct shmmap_state {
	vm_offset_t va;
	int shmid;
};

static void shm_deallocate_segment(struct shmid_kernel *);
static int shm_find_segment_by_key(key_t);
static struct shmid_kernel *shm_find_segment_by_shmid(int);
static struct shmid_kernel *shm_find_segment_by_shmidx(int);
static int shm_delete_mapping(struct vmspace *vm, struct shmmap_state *);
static void shmrealloc(void);
static void shminit(void);
static int sysvshm_modload(struct module *, int, void *);
static int shmunload(void);
static void shmexit_myhook(struct vmspace *vm);
static void shmfork_myhook(struct proc *p1, struct proc *p2);
static int sysctl_shmsegs(SYSCTL_HANDLER_ARGS);

/*
 * Tuneable values.
 */
#ifndef SHMMAXPGS
#define	SHMMAXPGS	8192	/* Note: sysv shared memory is swap backed. */
#endif
#ifndef SHMMAX
#define	SHMMAX	(SHMMAXPGS*PAGE_SIZE)
#endif
#ifndef SHMMIN
#define	SHMMIN	1
#endif
#ifndef SHMMNI
#define	SHMMNI	192
#endif
#ifndef SHMSEG
#define	SHMSEG	128
#endif
#ifndef SHMALL
#define	SHMALL	(SHMMAXPGS)
#endif

struct	shminfo shminfo = {
	SHMMAX,
	SHMMIN,
	SHMMNI,
	SHMSEG,
	SHMALL
};

static int shm_use_phys;
static int shm_allow_removed;

SYSCTL_DECL(_kern_ipc);
SYSCTL_ULONG(_kern_ipc, OID_AUTO, shmmax, CTLFLAG_RW, &shminfo.shmmax, 0,
    "Maximum shared memory segment size");
SYSCTL_ULONG(_kern_ipc, OID_AUTO, shmmin, CTLFLAG_RW, &shminfo.shmmin, 0,
    "Minimum shared memory segment size");
SYSCTL_ULONG(_kern_ipc, OID_AUTO, shmmni, CTLFLAG_RDTUN, &shminfo.shmmni, 0,
    "Number of shared memory identifiers");
SYSCTL_ULONG(_kern_ipc, OID_AUTO, shmseg, CTLFLAG_RDTUN, &shminfo.shmseg, 0,
    "Number of segments per process");
SYSCTL_ULONG(_kern_ipc, OID_AUTO, shmall, CTLFLAG_RW, &shminfo.shmall, 0,
    "Maximum number of pages available for shared memory");
SYSCTL_INT(_kern_ipc, OID_AUTO, shm_use_phys, CTLFLAG_RW,
    &shm_use_phys, 0, "Enable/Disable locking of shared memory pages in core");
SYSCTL_INT(_kern_ipc, OID_AUTO, shm_allow_removed, CTLFLAG_RW,
    &shm_allow_removed, 0,
    "Enable/Disable attachment to attached segments marked for removal");
SYSCTL_PROC(_kern_ipc, OID_AUTO, shmsegs, CTLFLAG_RD,
    NULL, 0, sysctl_shmsegs, "",
    "Current number of shared memory segments allocated");

static int
shm_find_segment_by_key(key)
	key_t key;
{
	int i;

	for (i = 0; i < shmalloced; i++)
		if ((shmsegs[i].u.shm_perm.mode & SHMSEG_ALLOCATED) &&
		    shmsegs[i].u.shm_perm.key == key)
			return (i);
	return (-1);
}

static struct shmid_kernel *
shm_find_segment_by_shmid(int shmid)
{
	int segnum;
	struct shmid_kernel *shmseg;

	segnum = IPCID_TO_IX(shmid);
	if (segnum < 0 || segnum >= shmalloced)
		return (NULL);
	shmseg = &shmsegs[segnum];
	if ((shmseg->u.shm_perm.mode & SHMSEG_ALLOCATED) == 0 ||
	    (!shm_allow_removed &&
	     (shmseg->u.shm_perm.mode & SHMSEG_REMOVED) != 0) ||
	    shmseg->u.shm_perm.seq != IPCID_TO_SEQ(shmid))
		return (NULL);
	return (shmseg);
}

static struct shmid_kernel *
shm_find_segment_by_shmidx(int segnum)
{
	struct shmid_kernel *shmseg;

	if (segnum < 0 || segnum >= shmalloced)
		return (NULL);
	shmseg = &shmsegs[segnum];
	if ((shmseg->u.shm_perm.mode & SHMSEG_ALLOCATED) == 0 ||
	    (!shm_allow_removed &&
	     (shmseg->u.shm_perm.mode & SHMSEG_REMOVED) != 0))
		return (NULL);
	return (shmseg);
}

static void
shm_deallocate_segment(shmseg)
	struct shmid_kernel *shmseg;
{
	size_t size;

	GIANT_REQUIRED;

	vm_object_deallocate(shmseg->u.shm_internal);
	shmseg->u.shm_internal = NULL;
	size = round_page(shmseg->u.shm_segsz);
	shm_committed -= btoc(size);
	shm_nused--;
	shmseg->u.shm_perm.mode = SHMSEG_FREE;
#ifdef MAC
	mac_cleanup_sysv_shm(shmseg);
#endif
}

static int
shm_delete_mapping(struct vmspace *vm, struct shmmap_state *shmmap_s)
{
	struct shmid_kernel *shmseg;
	int segnum, result;
	size_t size;

	GIANT_REQUIRED;

	segnum = IPCID_TO_IX(shmmap_s->shmid);
	shmseg = &shmsegs[segnum];
	size = round_page(shmseg->u.shm_segsz);
	result = vm_map_remove(&vm->vm_map, shmmap_s->va, shmmap_s->va + size);
	if (result != KERN_SUCCESS)
		return (EINVAL);
	shmmap_s->shmid = -1;
	shmseg->u.shm_dtime = time_second;
	if ((--shmseg->u.shm_nattch <= 0) &&
	    (shmseg->u.shm_perm.mode & SHMSEG_REMOVED)) {
		shm_deallocate_segment(shmseg);
		shm_last_free = segnum;
	}
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct shmdt_args {
	const void *shmaddr;
};
#endif

/*
 * MPSAFE
 */
int
shmdt(td, uap)
	struct thread *td;
	struct shmdt_args *uap;
{
	struct proc *p = td->td_proc;
	struct shmmap_state *shmmap_s;
#ifdef MAC
	struct shmid_kernel *shmsegptr;
#endif
	int i;
	int error = 0;

	if (!jail_sysvipc_allowed && jailed(td->td_ucred))
		return (ENOSYS);
	mtx_lock(&Giant);
	shmmap_s = p->p_vmspace->vm_shm;
 	if (shmmap_s == NULL) {
		error = EINVAL;
		goto done2;
	}
	for (i = 0; i < shminfo.shmseg; i++, shmmap_s++) {
		if (shmmap_s->shmid != -1 &&
		    shmmap_s->va == (vm_offset_t)uap->shmaddr) {
			break;
		}
	}
	if (i == shminfo.shmseg) {
		error = EINVAL;
		goto done2;
	}
#ifdef MAC
	shmsegptr = &shmsegs[IPCID_TO_IX(shmmap_s->shmid)];
	error = mac_check_sysv_shmdt(td->td_ucred, shmsegptr);
	if (error != 0) {
		MPRINTF(("mac_check_sysv_shmdt returned %d\n", error));
		goto done2;
	}
#endif
	error = shm_delete_mapping(p->p_vmspace, shmmap_s);
done2:
	mtx_unlock(&Giant);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct shmat_args {
	int shmid;
	const void *shmaddr;
	int shmflg;
};
#endif

/*
 * MPSAFE
 */
int
kern_shmat(td, shmid, shmaddr, shmflg)
	struct thread *td;
	int shmid;
	const void *shmaddr;
	int shmflg;
{
	struct proc *p = td->td_proc;
	int i, flags;
	struct shmid_kernel *shmseg;
	struct shmmap_state *shmmap_s = NULL;
	vm_offset_t attach_va;
	vm_prot_t prot;
	vm_size_t size;
	int rv;
	int error = 0;

	if (!jail_sysvipc_allowed && jailed(td->td_ucred))
		return (ENOSYS);
	mtx_lock(&Giant);
	shmmap_s = p->p_vmspace->vm_shm;
	if (shmmap_s == NULL) {
		size = shminfo.shmseg * sizeof(struct shmmap_state);
		shmmap_s = malloc(size, M_SHM, M_WAITOK);
		for (i = 0; i < shminfo.shmseg; i++)
			shmmap_s[i].shmid = -1;
		p->p_vmspace->vm_shm = shmmap_s;
	}
	shmseg = shm_find_segment_by_shmid(shmid);
	if (shmseg == NULL) {
		error = EINVAL;
		goto done2;
	}
	error = ipcperm(td, &shmseg->u.shm_perm,
	    (shmflg & SHM_RDONLY) ? IPC_R : IPC_R|IPC_W);
	if (error)
		goto done2;
#ifdef MAC
	error = mac_check_sysv_shmat(td->td_ucred, shmseg, shmflg);
	if (error != 0) {
	 	MPRINTF(("mac_check_sysv_shmat returned %d\n", error));
		goto done2;
	}
#endif
	for (i = 0; i < shminfo.shmseg; i++) {
		if (shmmap_s->shmid == -1)
			break;
		shmmap_s++;
	}
	if (i >= shminfo.shmseg) {
		error = EMFILE;
		goto done2;
	}
	size = round_page(shmseg->u.shm_segsz);
#ifdef VM_PROT_READ_IS_EXEC
	prot = VM_PROT_READ | VM_PROT_EXECUTE;
#else
	prot = VM_PROT_READ;
#endif
	if ((shmflg & SHM_RDONLY) == 0)
		prot |= VM_PROT_WRITE;
	flags = MAP_ANON | MAP_SHARED;
	if (shmaddr) {
		flags |= MAP_FIXED;
		if (shmflg & SHM_RND) {
			attach_va = (vm_offset_t)shmaddr & ~(SHMLBA-1);
		} else if (((vm_offset_t)shmaddr & (SHMLBA-1)) == 0) {
			attach_va = (vm_offset_t)shmaddr;
		} else {
			error = EINVAL;
			goto done2;
		}
	} else {
		/*
		 * This is just a hint to vm_map_find() about where to
		 * put it.
		 */
		PROC_LOCK(p);
		attach_va = round_page((vm_offset_t)p->p_vmspace->vm_daddr +
		    lim_max(p, RLIMIT_DATA));
		PROC_UNLOCK(p);
	}

	vm_object_reference(shmseg->u.shm_internal);
	rv = vm_map_find(&p->p_vmspace->vm_map, shmseg->u.shm_internal,
		0, &attach_va, size, (flags & MAP_FIXED)?0:1, prot, prot, 0);
	if (rv != KERN_SUCCESS) {
		vm_object_deallocate(shmseg->u.shm_internal);
		error = ENOMEM;
		goto done2;
	}
	vm_map_inherit(&p->p_vmspace->vm_map,
		attach_va, attach_va + size, VM_INHERIT_SHARE);

	shmmap_s->va = attach_va;
	shmmap_s->shmid = shmid;
	shmseg->u.shm_lpid = p->p_pid;
	shmseg->u.shm_atime = time_second;
	shmseg->u.shm_nattch++;
	td->td_retval[0] = attach_va;
done2:
	mtx_unlock(&Giant);
	return (error);
}

int
shmat(td, uap)
	struct thread *td;
	struct shmat_args *uap;
{
	return kern_shmat(td, uap->shmid, uap->shmaddr, uap->shmflg);
}

struct oshmid_ds {
	struct	ipc_perm shm_perm;	/* operation perms */
	int	shm_segsz;		/* size of segment (bytes) */
	u_short	shm_cpid;		/* pid, creator */
	u_short	shm_lpid;		/* pid, last operation */
	short	shm_nattch;		/* no. of current attaches */
	time_t	shm_atime;		/* last attach time */
	time_t	shm_dtime;		/* last detach time */
	time_t	shm_ctime;		/* last change time */
	void	*shm_handle;		/* internal handle for shm segment */
};

struct oshmctl_args {
	int shmid;
	int cmd;
	struct oshmid_ds *ubuf;
};

/*
 * MPSAFE
 */
static int
oshmctl(td, uap)
	struct thread *td;
	struct oshmctl_args *uap;
{
#ifdef COMPAT_43
	int error = 0;
	struct shmid_kernel *shmseg;
	struct oshmid_ds outbuf;

	if (!jail_sysvipc_allowed && jailed(td->td_ucred))
		return (ENOSYS);
	mtx_lock(&Giant);
	shmseg = shm_find_segment_by_shmid(uap->shmid);
	if (shmseg == NULL) {
		error = EINVAL;
		goto done2;
	}
	switch (uap->cmd) {
	case IPC_STAT:
		error = ipcperm(td, &shmseg->u.shm_perm, IPC_R);
		if (error)
			goto done2;
#ifdef MAC
		error = mac_check_sysv_shmctl(td->td_ucred, shmseg, uap->cmd);
		if (error != 0) {
			MPRINTF(("mac_check_sysv_shmctl returned %d\n",
			    error));
			goto done2;
		}
#endif
		outbuf.shm_perm = shmseg->u.shm_perm;
		outbuf.shm_segsz = shmseg->u.shm_segsz;
		outbuf.shm_cpid = shmseg->u.shm_cpid;
		outbuf.shm_lpid = shmseg->u.shm_lpid;
		outbuf.shm_nattch = shmseg->u.shm_nattch;
		outbuf.shm_atime = shmseg->u.shm_atime;
		outbuf.shm_dtime = shmseg->u.shm_dtime;
		outbuf.shm_ctime = shmseg->u.shm_ctime;
		outbuf.shm_handle = shmseg->u.shm_internal;
		error = copyout(&outbuf, uap->ubuf, sizeof(outbuf));
		if (error)
			goto done2;
		break;
	default:
		error = shmctl(td, (struct shmctl_args *)uap);
		break;
	}
done2:
	mtx_unlock(&Giant);
	return (error);
#else
	return (EINVAL);
#endif
}

#ifndef _SYS_SYSPROTO_H_
struct shmctl_args {
	int shmid;
	int cmd;
	struct shmid_ds *buf;
};
#endif

/*
 * MPSAFE
 */
int
kern_shmctl(td, shmid, cmd, buf, bufsz)
	struct thread *td;
	int shmid;
	int cmd;
	void *buf;
	size_t *bufsz;
{
	int error = 0;
	struct shmid_kernel *shmseg;

	if (!jail_sysvipc_allowed && jailed(td->td_ucred))
		return (ENOSYS);

	mtx_lock(&Giant);
	switch (cmd) {
	case IPC_INFO:
		memcpy(buf, &shminfo, sizeof(shminfo));
		if (bufsz)
			*bufsz = sizeof(shminfo);
		td->td_retval[0] = shmalloced;
		goto done2;
	case SHM_INFO: {
		struct shm_info shm_info;
		shm_info.used_ids = shm_nused;
		shm_info.shm_rss = 0;	/*XXX where to get from ? */
		shm_info.shm_tot = 0;	/*XXX where to get from ? */
		shm_info.shm_swp = 0;	/*XXX where to get from ? */
		shm_info.swap_attempts = 0;	/*XXX where to get from ? */
		shm_info.swap_successes = 0;	/*XXX where to get from ? */
		memcpy(buf, &shm_info, sizeof(shm_info));
		if (bufsz)
			*bufsz = sizeof(shm_info);
		td->td_retval[0] = shmalloced;
		goto done2;
	}
	}
	if (cmd == SHM_STAT)
		shmseg = shm_find_segment_by_shmidx(shmid);
	else
		shmseg = shm_find_segment_by_shmid(shmid);
	if (shmseg == NULL) {
		error = EINVAL;
		goto done2;
	}
#ifdef MAC
	error = mac_check_sysv_shmctl(td->td_ucred, shmseg, cmd);
	if (error != 0) {
		MPRINTF(("mac_check_sysv_shmctl returned %d\n", error));
		goto done2;
	}
#endif
	switch (cmd) {
	case SHM_STAT:
	case IPC_STAT:
		error = ipcperm(td, &shmseg->u.shm_perm, IPC_R);
		if (error)
			goto done2;
		memcpy(buf, &shmseg->u, sizeof(struct shmid_ds));
		if (bufsz)
			*bufsz = sizeof(struct shmid_ds);
		if (cmd == SHM_STAT)
			td->td_retval[0] = IXSEQ_TO_IPCID(shmid, shmseg->u.shm_perm);
		break;
	case IPC_SET: {
		struct shmid_ds *shmid;

		shmid = (struct shmid_ds *)buf;
		error = ipcperm(td, &shmseg->u.shm_perm, IPC_M);
		if (error)
			goto done2;
		shmseg->u.shm_perm.uid = shmid->shm_perm.uid;
		shmseg->u.shm_perm.gid = shmid->shm_perm.gid;
		shmseg->u.shm_perm.mode =
		    (shmseg->u.shm_perm.mode & ~ACCESSPERMS) |
		    (shmid->shm_perm.mode & ACCESSPERMS);
		shmseg->u.shm_ctime = time_second;
		break;
	}
	case IPC_RMID:
		error = ipcperm(td, &shmseg->u.shm_perm, IPC_M);
		if (error)
			goto done2;
		shmseg->u.shm_perm.key = IPC_PRIVATE;
		shmseg->u.shm_perm.mode |= SHMSEG_REMOVED;
		if (shmseg->u.shm_nattch <= 0) {
			shm_deallocate_segment(shmseg);
			shm_last_free = IPCID_TO_IX(shmid);
		}
		break;
#if 0
	case SHM_LOCK:
	case SHM_UNLOCK:
#endif
	default:
		error = EINVAL;
		break;
	}
done2:
	mtx_unlock(&Giant);
	return (error);
}

int
shmctl(td, uap)
	struct thread *td;
	struct shmctl_args *uap;
{
	int error = 0;
	struct shmid_ds buf;
	size_t bufsz;
	
	/* IPC_SET needs to copyin the buffer before calling kern_shmctl */
	if (uap->cmd == IPC_SET) {
		if ((error = copyin(uap->buf, &buf, sizeof(struct shmid_ds))))
			goto done;
	}
	
	error = kern_shmctl(td, uap->shmid, uap->cmd, (void *)&buf, &bufsz);
	if (error)
		goto done;
	
	/* Cases in which we need to copyout */
	switch (uap->cmd) {
	case IPC_INFO:
	case SHM_INFO:
	case SHM_STAT:
	case IPC_STAT:
		error = copyout(&buf, uap->buf, bufsz);
		break;
	}

done:
	if (error) {
		/* Invalidate the return value */
		td->td_retval[0] = -1;
	}
	return (error);
}


#ifndef _SYS_SYSPROTO_H_
struct shmget_args {
	key_t key;
	size_t size;
	int shmflg;
};
#endif

static int
shmget_existing(td, uap, mode, segnum)
	struct thread *td;
	struct shmget_args *uap;
	int mode;
	int segnum;
{
	struct shmid_kernel *shmseg;
	int error;

	shmseg = &shmsegs[segnum];
	if (shmseg->u.shm_perm.mode & SHMSEG_REMOVED) {
		/*
		 * This segment is in the process of being allocated.  Wait
		 * until it's done, and look the key up again (in case the
		 * allocation failed or it was freed).
		 */
		shmseg->u.shm_perm.mode |= SHMSEG_WANTED;
		error = tsleep(shmseg, PLOCK | PCATCH, "shmget", 0);
		if (error)
			return (error);
		return (EAGAIN);
	}
	if ((uap->shmflg & (IPC_CREAT | IPC_EXCL)) == (IPC_CREAT | IPC_EXCL))
		return (EEXIST);
#ifdef MAC
	error = mac_check_sysv_shmget(td->td_ucred, shmseg, uap->shmflg);
	if (error != 0) {
		MPRINTF(("mac_check_sysv_shmget returned %d\n", error));
		return (error);
	}
#endif
	error = ipcperm(td, &shmseg->u.shm_perm, mode);
	if (error)
		return (error);
	if (uap->size && uap->size > shmseg->u.shm_segsz)
		return (EINVAL);
	td->td_retval[0] = IXSEQ_TO_IPCID(segnum, shmseg->u.shm_perm);
	return (0);
}

static int
shmget_allocate_segment(td, uap, mode)
	struct thread *td;
	struct shmget_args *uap;
	int mode;
{
	int i, segnum, shmid, size;
	struct ucred *cred = td->td_ucred;
	struct shmid_kernel *shmseg;
	vm_object_t shm_object;

	GIANT_REQUIRED;

	if (uap->size < shminfo.shmmin || uap->size > shminfo.shmmax)
		return (EINVAL);
	if (shm_nused >= shminfo.shmmni) /* Any shmids left? */
		return (ENOSPC);
	size = round_page(uap->size);
	if (shm_committed + btoc(size) > shminfo.shmall)
		return (ENOMEM);
	if (shm_last_free < 0) {
		shmrealloc();	/* Maybe expand the shmsegs[] array. */
		for (i = 0; i < shmalloced; i++)
			if (shmsegs[i].u.shm_perm.mode & SHMSEG_FREE)
				break;
		if (i == shmalloced)
			return (ENOSPC);
		segnum = i;
	} else  {
		segnum = shm_last_free;
		shm_last_free = -1;
	}
	shmseg = &shmsegs[segnum];
	/*
	 * In case we sleep in malloc(), mark the segment present but deleted
	 * so that noone else tries to create the same key.
	 */
	shmseg->u.shm_perm.mode = SHMSEG_ALLOCATED | SHMSEG_REMOVED;
	shmseg->u.shm_perm.key = uap->key;
	shmseg->u.shm_perm.seq = (shmseg->u.shm_perm.seq + 1) & 0x7fff;
	shmid = IXSEQ_TO_IPCID(segnum, shmseg->u.shm_perm);
	
	/*
	 * We make sure that we have allocated a pager before we need
	 * to.
	 */
	if (shm_use_phys) {
		shm_object =
		    vm_pager_allocate(OBJT_PHYS, 0, size, VM_PROT_DEFAULT, 0);
	} else {
		shm_object =
		    vm_pager_allocate(OBJT_SWAP, 0, size, VM_PROT_DEFAULT, 0);
	}
	VM_OBJECT_LOCK(shm_object);
	vm_object_clear_flag(shm_object, OBJ_ONEMAPPING);
	vm_object_set_flag(shm_object, OBJ_NOSPLIT);
	VM_OBJECT_UNLOCK(shm_object);

	shmseg->u.shm_internal = shm_object;
	shmseg->u.shm_perm.cuid = shmseg->u.shm_perm.uid = cred->cr_uid;
	shmseg->u.shm_perm.cgid = shmseg->u.shm_perm.gid = cred->cr_gid;
	shmseg->u.shm_perm.mode = (shmseg->u.shm_perm.mode & SHMSEG_WANTED) |
	    (mode & ACCESSPERMS) | SHMSEG_ALLOCATED;
	shmseg->u.shm_segsz = uap->size;
	shmseg->u.shm_cpid = td->td_proc->p_pid;
	shmseg->u.shm_lpid = shmseg->u.shm_nattch = 0;
	shmseg->u.shm_atime = shmseg->u.shm_dtime = 0;
#ifdef MAC
	mac_create_sysv_shm(cred, shmseg);
#endif
	shmseg->u.shm_ctime = time_second;
	shm_committed += btoc(size);
	shm_nused++;
	if (shmseg->u.shm_perm.mode & SHMSEG_WANTED) {
		/*
		 * Somebody else wanted this key while we were asleep.  Wake
		 * them up now.
		 */
		shmseg->u.shm_perm.mode &= ~SHMSEG_WANTED;
		wakeup(shmseg);
	}
	td->td_retval[0] = shmid;
	return (0);
}

/*
 * MPSAFE
 */
int
shmget(td, uap)
	struct thread *td;
	struct shmget_args *uap;
{
	int segnum, mode;
	int error;

	if (!jail_sysvipc_allowed && jailed(td->td_ucred))
		return (ENOSYS);
	mtx_lock(&Giant);
	mode = uap->shmflg & ACCESSPERMS;
	if (uap->key != IPC_PRIVATE) {
	again:
		segnum = shm_find_segment_by_key(uap->key);
		if (segnum >= 0) {
			error = shmget_existing(td, uap, mode, segnum);
			if (error == EAGAIN)
				goto again;
			goto done2;
		}
		if ((uap->shmflg & IPC_CREAT) == 0) {
			error = ENOENT;
			goto done2;
		}
	}
	error = shmget_allocate_segment(td, uap, mode);
done2:
	mtx_unlock(&Giant);
	return (error);
}

/*
 * MPSAFE
 */
int
shmsys(td, uap)
	struct thread *td;
	/* XXX actually varargs. */
	struct shmsys_args /* {
		int	which;
		int	a2;
		int	a3;
		int	a4;
	} */ *uap;
{
	int error;

	if (!jail_sysvipc_allowed && jailed(td->td_ucred))
		return (ENOSYS);
	if (uap->which < 0 ||
	    uap->which >= sizeof(shmcalls)/sizeof(shmcalls[0]))
		return (EINVAL);
	mtx_lock(&Giant);
	error = (*shmcalls[uap->which])(td, &uap->a2);
	mtx_unlock(&Giant);
	return (error);
}

static void
shmfork_myhook(p1, p2)
	struct proc *p1, *p2;
{
	struct shmmap_state *shmmap_s;
	size_t size;
	int i;

	mtx_lock(&Giant);
	size = shminfo.shmseg * sizeof(struct shmmap_state);
	shmmap_s = malloc(size, M_SHM, M_WAITOK);
	bcopy(p1->p_vmspace->vm_shm, shmmap_s, size);
	p2->p_vmspace->vm_shm = shmmap_s;
	for (i = 0; i < shminfo.shmseg; i++, shmmap_s++)
		if (shmmap_s->shmid != -1)
			shmsegs[IPCID_TO_IX(shmmap_s->shmid)].u.shm_nattch++;
	mtx_unlock(&Giant);
}

static void
shmexit_myhook(struct vmspace *vm)
{
	struct shmmap_state *base, *shm;
	int i;

	if ((base = vm->vm_shm) != NULL) {
		vm->vm_shm = NULL;
		mtx_lock(&Giant);
		for (i = 0, shm = base; i < shminfo.shmseg; i++, shm++) {
			if (shm->shmid != -1)
				shm_delete_mapping(vm, shm);
		}
		mtx_unlock(&Giant);
		free(base, M_SHM);
	}
}

static void
shmrealloc(void)
{
	int i;
	struct shmid_kernel *newsegs;

	if (shmalloced >= shminfo.shmmni)
		return;

	newsegs = malloc(shminfo.shmmni * sizeof(*newsegs), M_SHM, M_WAITOK);
	if (newsegs == NULL)
		return;
	for (i = 0; i < shmalloced; i++)
		bcopy(&shmsegs[i], &newsegs[i], sizeof(newsegs[0]));
	for (; i < shminfo.shmmni; i++) {
		shmsegs[i].u.shm_perm.mode = SHMSEG_FREE;
		shmsegs[i].u.shm_perm.seq = 0;
#ifdef MAC
		mac_init_sysv_shm(&shmsegs[i]);
#endif
	}
	free(shmsegs, M_SHM);
	shmsegs = newsegs;
	shmalloced = shminfo.shmmni;
}

static void
shminit()
{
	int i;

	TUNABLE_ULONG_FETCH("kern.ipc.shmmaxpgs", &shminfo.shmall);
	for (i = PAGE_SIZE; i > 0; i--) {
		shminfo.shmmax = shminfo.shmall * i;
		if (shminfo.shmmax >= shminfo.shmall)
			break;
	}
	TUNABLE_ULONG_FETCH("kern.ipc.shmmin", &shminfo.shmmin);
	TUNABLE_ULONG_FETCH("kern.ipc.shmmni", &shminfo.shmmni);
	TUNABLE_ULONG_FETCH("kern.ipc.shmseg", &shminfo.shmseg);
	TUNABLE_INT_FETCH("kern.ipc.shm_use_phys", &shm_use_phys);

	shmalloced = shminfo.shmmni;
	shmsegs = malloc(shmalloced * sizeof(shmsegs[0]), M_SHM, M_WAITOK);
	if (shmsegs == NULL)
		panic("cannot allocate initial memory for sysvshm");
	for (i = 0; i < shmalloced; i++) {
		shmsegs[i].u.shm_perm.mode = SHMSEG_FREE;
		shmsegs[i].u.shm_perm.seq = 0;
#ifdef MAC
		mac_init_sysv_shm(&shmsegs[i]);
#endif
	}
	shm_last_free = 0;
	shm_nused = 0;
	shm_committed = 0;
	shmexit_hook = &shmexit_myhook;
	shmfork_hook = &shmfork_myhook;
}

static int
shmunload()
{
#ifdef MAC
	int i;	
#endif

	if (shm_nused > 0)
		return (EBUSY);

#ifdef MAC
	for (i = 0; i < shmalloced; i++)
		mac_destroy_sysv_shm(&shmsegs[i]);
#endif
	free(shmsegs, M_SHM);
	shmexit_hook = NULL;
	shmfork_hook = NULL;
	return (0);
}

static int
sysctl_shmsegs(SYSCTL_HANDLER_ARGS)
{

	return (SYSCTL_OUT(req, shmsegs, shmalloced * sizeof(shmsegs[0])));
}

static int
sysvshm_modload(struct module *module, int cmd, void *arg)
{
	int error = 0;

	switch (cmd) {
	case MOD_LOAD:
		shminit();
		break;
	case MOD_UNLOAD:
		error = shmunload();
		break;
	case MOD_SHUTDOWN:
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

static moduledata_t sysvshm_mod = {
	"sysvshm",
	&sysvshm_modload,
	NULL
};

SYSCALL_MODULE_HELPER(shmsys);
SYSCALL_MODULE_HELPER(shmat);
SYSCALL_MODULE_HELPER(shmctl);
SYSCALL_MODULE_HELPER(shmdt);
SYSCALL_MODULE_HELPER(shmget);

DECLARE_MODULE(sysvshm, sysvshm_mod,
	SI_SUB_SYSV_SHM, SI_ORDER_FIRST);
MODULE_VERSION(sysvshm, 1);

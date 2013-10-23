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
/*-
 * VPS adaption:
 *
 * Copyright (c) 2009-2013 Klaus P. Ohrhallinger <k@7he.at>
 * All rights reserved.
 *
 * Development of this software was partly funded by:
 *    TransIP.nl <http://www.transip.nl/>
 *
 * <BSD license>
 *
 * $Id: sysv_shm.c 162 2013-06-06 18:17:55Z klaus $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"
#include "opt_sysvipc.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/eventhandler.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/sysctl.h>
#include <sys/shm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/racct.h>
#include <sys/resourcevar.h>
#include <sys/rwlock.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/syscallsubr.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/jail.h>

#include <vps/vps.h>
#include <vps/vps2.h>
#include <vps/vps_int.h>
#include <vps/vps_libdump.h>
#define _VPS_SNAPST_H_RESTORE_OBJ
#include <vps/vps_snapst.h>

#include <security/mac/mac_framework.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_object.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>

FEATURE(sysv_shm, "System V shared memory segments support");

static MALLOC_DEFINE(M_SHM, "shm", "SVID compatible shared memory segments");

static int shmget_allocate_segment(struct thread *td,
    struct shmget_args *uap, int mode);
static int shmget_existing(struct thread *td, struct shmget_args *uap,
    int mode, int segnum);

#define	SHMSEG_FREE     	0x0200
#define	SHMSEG_REMOVED  	0x0400
#define	SHMSEG_ALLOCATED	0x0800
#define	SHMSEG_WANTED		0x1000

#if 0
static int shm_last_free, shm_nused, shmalloced;
vm_size_t shm_committed;
static struct shmid_kernel	*shmsegs;

struct	shminfo shminfo = {
	SHMMAX,
	SHMMIN,
	SHMMNI,
	SHMSEG,
	SHMALL
};

static int shm_use_phys;
static int shm_allow_removed;
#endif

VPS_DEFINE(int, shm_last_free);
VPS_DEFINE(int, shm_nused);
VPS_DEFINE(int, shmalloced);
VPS_DEFINE(vm_size_t, shm_committed);
VPS_DEFINE(struct shmid_kernel *, shmsegs);
VPS_DEFINE(struct shminfo, shminfo);
VPS_DEFINE(int, shm_use_phys) = 0;
VPS_DEFINE(int, shm_allow_removed) = 0;

#define V_shm_last_free		VPSV(shm_last_free)
#define V_shm_nused		VPSV(shm_nused)
#define V_shmalloced		VPSV(shmalloced)
#define V_shm_committed		VPSV(shm_committed)
#define V_shmsegs		VPSV(shmsegs)
#define V_shminfo		VPSV(shminfo)
#define V_shm_use_phys		VPSV(shm_use_phys)
#define V_shm_allow_removed	VPSV(shm_allow_removed)

#ifdef VPS
static eventhandler_tag shm_vpsalloc_tag;
static eventhandler_tag shm_vpsfree_tag;
static int shm_nused_global;
#endif

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
static int shminit(void);
static int sysvshm_modload(struct module *, int, void *);
static int shmunload(void);
static void shmexit_myhook(struct vmspace *vm);
static void shmfork_myhook(struct proc *p1, struct proc *p2);
static int sysctl_shmsegs(SYSCTL_HANDLER_ARGS);

/*
 * Tuneable values.
 */
#ifndef SHMMAXPGS
#define	SHMMAXPGS	131072	/* Note: sysv shared memory is swap backed. */
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

#if 0
struct	shminfo shminfo = {
	SHMMAX,
	SHMMIN,
	SHMMNI,
	SHMSEG,
	SHMALL
};

static int shm_use_phys;
static int shm_allow_removed;
#endif

SYSCTL_VPS_ULONG(_kern_ipc, OID_AUTO, shmmax, CTLFLAG_RW, &VPS_NAME(shminfo.shmmax), 0,
    "Maximum shared memory segment size");
SYSCTL_VPS_ULONG(_kern_ipc, OID_AUTO, shmmin, CTLFLAG_RW, &VPS_NAME(shminfo.shmmin), 0,
    "Minimum shared memory segment size");
SYSCTL_VPS_ULONG(_kern_ipc, OID_AUTO, shmmni, CTLFLAG_RDTUN, &VPS_NAME(shminfo.shmmni), 0,
    "Number of shared memory identifiers");
SYSCTL_VPS_ULONG(_kern_ipc, OID_AUTO, shmseg, CTLFLAG_RDTUN, &VPS_NAME(shminfo.shmseg), 0,
    "Number of segments per process");
SYSCTL_VPS_ULONG(_kern_ipc, OID_AUTO, shmall, CTLFLAG_RW, &VPS_NAME(shminfo.shmall), 0,
    "Maximum number of pages available for shared memory");
SYSCTL_VPS_INT(_kern_ipc, OID_AUTO, shm_use_phys, CTLFLAG_RW,
    &VPS_NAME(shm_use_phys), 0, "Enable/Disable locking of shared memory pages in core");
SYSCTL_VPS_INT(_kern_ipc, OID_AUTO, shm_allow_removed, CTLFLAG_RW,
    &VPS_NAME(shm_allow_removed), 0,
    "Enable/Disable attachment to attached segments marked for removal");
SYSCTL_VPS_PROC(_kern_ipc, OID_AUTO, shmsegs, CTLTYPE_OPAQUE | CTLFLAG_RD,
    NULL, 0, sysctl_shmsegs, "",
    "Current number of shared memory segments allocated");

static int
shm_find_segment_by_key(key)
	key_t key;
{
	int i;

	for (i = 0; i < V_shmalloced; i++)
		if ((V_shmsegs[i].u.shm_perm.mode & SHMSEG_ALLOCATED) &&
		    V_shmsegs[i].u.shm_perm.key == key)
			return (i);
	return (-1);
}

static struct shmid_kernel *
shm_find_segment_by_shmid(int shmid)
{
	int segnum;
	struct shmid_kernel *shmseg;

	segnum = IPCID_TO_IX(shmid);
	if (segnum < 0 || segnum >= V_shmalloced)
		return (NULL);
	shmseg = &V_shmsegs[segnum];
	if ((shmseg->u.shm_perm.mode & SHMSEG_ALLOCATED) == 0 ||
	    (!V_shm_allow_removed &&
	     (shmseg->u.shm_perm.mode & SHMSEG_REMOVED) != 0) ||
	    shmseg->u.shm_perm.seq != IPCID_TO_SEQ(shmid))
		return (NULL);
	return (shmseg);
}

static struct shmid_kernel *
shm_find_segment_by_shmidx(int segnum)
{
	struct shmid_kernel *shmseg;

	if (segnum < 0 || segnum >= V_shmalloced)
		return (NULL);
	shmseg = &V_shmsegs[segnum];
	if ((shmseg->u.shm_perm.mode & SHMSEG_ALLOCATED) == 0 ||
	    (!V_shm_allow_removed &&
	     (shmseg->u.shm_perm.mode & SHMSEG_REMOVED) != 0))
		return (NULL);
	return (shmseg);
}

static void
shm_deallocate_segment(shmseg)
	struct shmid_kernel *shmseg;
{
	vm_size_t size;

	GIANT_REQUIRED;

	vm_object_deallocate(shmseg->object);
	shmseg->object = NULL;
	size = round_page(shmseg->u.shm_segsz);
	V_shm_committed -= btoc(size);
	V_shm_nused--;
#ifdef VPS
	atomic_subtract_int(&shm_nused_global, 1);
#endif
	shmseg->u.shm_perm.mode = SHMSEG_FREE;
#ifdef MAC
	mac_sysvshm_cleanup(shmseg);
#endif
	racct_sub_cred(shmseg->cred, RACCT_NSHM, 1);
	racct_sub_cred(shmseg->cred, RACCT_SHMSIZE, size);
	crfree(shmseg->cred);
	shmseg->cred = NULL;
}

static int
shm_delete_mapping(struct vmspace *vm, struct shmmap_state *shmmap_s)
{
	struct shmid_kernel *shmseg;
	int segnum, result;
	vm_size_t size;

	GIANT_REQUIRED;

	segnum = IPCID_TO_IX(shmmap_s->shmid);
	shmseg = &V_shmsegs[segnum];
	size = round_page(shmseg->u.shm_segsz);
	result = vm_map_remove(&vm->vm_map, shmmap_s->va, shmmap_s->va + size);
	if (result != KERN_SUCCESS)
		return (EINVAL);
	shmmap_s->shmid = -1;
	shmseg->u.shm_dtime = time_second;
	if ((--shmseg->u.shm_nattch <= 0) &&
	    (shmseg->u.shm_perm.mode & SHMSEG_REMOVED)) {
		shm_deallocate_segment(shmseg);
		V_shm_last_free = segnum;
	}
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct shmdt_args {
	const void *shmaddr;
};
#endif
int
sys_shmdt(td, uap)
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

	if (!prison_allow(td->td_ucred, PR_ALLOW_SYSVIPC))
		return (ENOSYS);
	mtx_lock(&Giant);
	shmmap_s = p->p_vmspace->vm_shm;
 	if (shmmap_s == NULL) {
		error = EINVAL;
		goto done2;
	}
	for (i = 0; i < V_shminfo.shmseg; i++, shmmap_s++) {
		if (shmmap_s->shmid != -1 &&
		    shmmap_s->va == (vm_offset_t)uap->shmaddr) {
			break;
		}
	}
	if (i == V_shminfo.shmseg) {
		error = EINVAL;
		goto done2;
	}
#ifdef MAC
	shmsegptr = &V_shmsegs[IPCID_TO_IX(shmmap_s->shmid)];
	error = mac_sysvshm_check_shmdt(td->td_ucred, shmsegptr);
	if (error != 0)
		goto done2;
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

	if (!prison_allow(td->td_ucred, PR_ALLOW_SYSVIPC))
		return (ENOSYS);
	mtx_lock(&Giant);
	shmmap_s = p->p_vmspace->vm_shm;
	if (shmmap_s == NULL) {
		shmmap_s = malloc(V_shminfo.shmseg * sizeof(struct shmmap_state),
		    M_SHM, M_WAITOK);
		for (i = 0; i < V_shminfo.shmseg; i++)
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
	error = mac_sysvshm_check_shmat(td->td_ucred, shmseg, shmflg);
	if (error != 0)
		goto done2;
#endif
	for (i = 0; i < V_shminfo.shmseg; i++) {
		if (shmmap_s->shmid == -1)
			break;
		shmmap_s++;
	}
	if (i >= V_shminfo.shmseg) {
		error = EMFILE;
		goto done2;
	}
	size = round_page(shmseg->u.shm_segsz);
	prot = VM_PROT_READ;
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

	vm_object_reference(shmseg->object);
	rv = vm_map_find(&p->p_vmspace->vm_map, shmseg->object,
	    0, &attach_va, size, (flags & MAP_FIXED) ? VMFS_NO_SPACE :
	    VMFS_OPTIMAL_SPACE, prot, prot, MAP_INHERIT_SHARE);
	if (rv != KERN_SUCCESS) {
		vm_object_deallocate(shmseg->object);
		error = ENOMEM;
		goto done2;
	}

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
sys_shmat(td, uap)
	struct thread *td;
	struct shmat_args *uap;
{
	return kern_shmat(td, uap->shmid, uap->shmaddr, uap->shmflg);
}

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

	if (!prison_allow(td->td_ucred, PR_ALLOW_SYSVIPC))
		return (ENOSYS);

	mtx_lock(&Giant);
	switch (cmd) {
	/*
	 * It is possible that kern_shmctl is being called from the Linux ABI
	 * layer, in which case, we will need to implement IPC_INFO.  It should
	 * be noted that other shmctl calls will be funneled through here for
	 * Linix binaries as well.
	 *
	 * NB: The Linux ABI layer will convert this data to structure(s) more
	 * consistent with the Linux ABI.
	 */
	case IPC_INFO:
		memcpy(buf, &V_shminfo, sizeof(V_shminfo));
		if (bufsz)
			*bufsz = sizeof(V_shminfo);
		td->td_retval[0] = V_shmalloced;
		goto done2;
	case SHM_INFO: {
		struct shm_info shm_info;
		shm_info.used_ids = V_shm_nused;
		shm_info.shm_rss = 0;	/*XXX where to get from ? */
		shm_info.shm_tot = 0;	/*XXX where to get from ? */
		shm_info.shm_swp = 0;	/*XXX where to get from ? */
		shm_info.swap_attempts = 0;	/*XXX where to get from ? */
		shm_info.swap_successes = 0;	/*XXX where to get from ? */
		memcpy(buf, &shm_info, sizeof(shm_info));
		if (bufsz)
			*bufsz = sizeof(shm_info);
		td->td_retval[0] = V_shmalloced;
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
	error = mac_sysvshm_check_shmctl(td->td_ucred, shmseg, cmd);
	if (error != 0)
		goto done2;
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
			V_shm_last_free = IPCID_TO_IX(shmid);
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

#ifndef _SYS_SYSPROTO_H_
struct shmctl_args {
	int shmid;
	int cmd;
	struct shmid_ds *buf;
};
#endif
int
sys_shmctl(td, uap)
	struct thread *td;
	struct shmctl_args *uap;
{
	int error = 0;
	struct shmid_ds buf;
	size_t bufsz;
	
	/*
	 * The only reason IPC_INFO, SHM_INFO, SHM_STAT exists is to support
	 * Linux binaries.  If we see the call come through the FreeBSD ABI,
	 * return an error back to the user since we do not to support this.
	 */
	if (uap->cmd == IPC_INFO || uap->cmd == SHM_INFO ||
	    uap->cmd == SHM_STAT)
		return (EINVAL);

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


static int
shmget_existing(td, uap, mode, segnum)
	struct thread *td;
	struct shmget_args *uap;
	int mode;
	int segnum;
{
	struct shmid_kernel *shmseg;
	int error;

	shmseg = &V_shmsegs[segnum];
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
	error = mac_sysvshm_check_shmget(td->td_ucred, shmseg, uap->shmflg);
	if (error != 0)
		return (error);
#endif
	if (uap->size != 0 && uap->size > shmseg->u.shm_segsz)
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
	int i, segnum, shmid;
	size_t size;
	struct ucred *cred = td->td_ucred;
	struct shmid_kernel *shmseg;
	vm_object_t shm_object;

	GIANT_REQUIRED;

	if (uap->size < V_shminfo.shmmin || uap->size > V_shminfo.shmmax)
		return (EINVAL);
	if (V_shm_nused >= V_shminfo.shmmni) /* Any shmids left? */
		return (ENOSPC);
	size = round_page(uap->size);
	if (V_shm_committed + btoc(size) > V_shminfo.shmall)
		return (ENOMEM);
	if (V_shm_last_free < 0) {
		shmrealloc();	/* Maybe expand the shmsegs[] array. */
		for (i = 0; i < V_shmalloced; i++)
			if (V_shmsegs[i].u.shm_perm.mode & SHMSEG_FREE)
				break;
		if (i == V_shmalloced)
			return (ENOSPC);
		segnum = i;
	} else  {
		segnum = V_shm_last_free;
		V_shm_last_free = -1;
	}
	shmseg = &V_shmsegs[segnum];
#ifdef RACCT
	PROC_LOCK(td->td_proc);
	if (racct_add(td->td_proc, RACCT_NSHM, 1)) {
		PROC_UNLOCK(td->td_proc);
		return (ENOSPC);
	}
	if (racct_add(td->td_proc, RACCT_SHMSIZE, size)) {
		racct_sub(td->td_proc, RACCT_NSHM, 1);
		PROC_UNLOCK(td->td_proc);
		return (ENOMEM);
	}
	PROC_UNLOCK(td->td_proc);
#endif
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
	shm_object = vm_pager_allocate(V_shm_use_phys ? OBJT_PHYS : OBJT_SWAP,
	    0, size, VM_PROT_DEFAULT, 0, cred);
	if (shm_object == NULL) {
#ifdef RACCT
		PROC_LOCK(td->td_proc);
		racct_sub(td->td_proc, RACCT_NSHM, 1);
		racct_sub(td->td_proc, RACCT_SHMSIZE, size);
		PROC_UNLOCK(td->td_proc);
#endif
		return (ENOMEM);
	}
	VM_OBJECT_WLOCK(shm_object);
	vm_object_clear_flag(shm_object, OBJ_ONEMAPPING);
	vm_object_set_flag(shm_object, OBJ_NOSPLIT);
	VM_OBJECT_WUNLOCK(shm_object);

	shmseg->object = shm_object;
	shmseg->u.shm_perm.cuid = shmseg->u.shm_perm.uid = cred->cr_uid;
	shmseg->u.shm_perm.cgid = shmseg->u.shm_perm.gid = cred->cr_gid;
	shmseg->u.shm_perm.mode = (shmseg->u.shm_perm.mode & SHMSEG_WANTED) |
	    (mode & ACCESSPERMS) | SHMSEG_ALLOCATED;
	shmseg->cred = crhold(cred);
	shmseg->u.shm_segsz = uap->size;
	shmseg->u.shm_cpid = td->td_proc->p_pid;
	shmseg->u.shm_lpid = shmseg->u.shm_nattch = 0;
	shmseg->u.shm_atime = shmseg->u.shm_dtime = 0;
#ifdef MAC
	mac_sysvshm_create(cred, shmseg);
#endif
	shmseg->u.shm_ctime = time_second;
	V_shm_committed += btoc(size);
	V_shm_nused++;
#ifdef VPS
	atomic_add_int(&shm_nused_global, 1);
#endif
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

#ifndef _SYS_SYSPROTO_H_
struct shmget_args {
	key_t key;
	size_t size;
	int shmflg;
};
#endif
int
sys_shmget(td, uap)
	struct thread *td;
	struct shmget_args *uap;
{
	int segnum, mode;
	int error;

	if (!prison_allow(td->td_ucred, PR_ALLOW_SYSVIPC))
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

static void
shmfork_myhook(p1, p2)
	struct proc *p1, *p2;
{
	struct shmmap_state *shmmap_s;
	size_t size;
	int i;

	mtx_lock(&Giant);
	size = V_shminfo.shmseg * sizeof(struct shmmap_state);
	shmmap_s = malloc(size, M_SHM, M_WAITOK);
	bcopy(p1->p_vmspace->vm_shm, shmmap_s, size);
	p2->p_vmspace->vm_shm = shmmap_s;
	for (i = 0; i < V_shminfo.shmseg; i++, shmmap_s++)
		if (shmmap_s->shmid != -1)
			V_shmsegs[IPCID_TO_IX(shmmap_s->shmid)].u.shm_nattch++;
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
		for (i = 0, shm = base; i < V_shminfo.shmseg; i++, shm++) {
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

	if (V_shmalloced >= V_shminfo.shmmni)
		return;

	newsegs = malloc(V_shminfo.shmmni * sizeof(*newsegs), M_SHM, M_WAITOK);
	if (newsegs == NULL)
		return;
	for (i = 0; i < V_shmalloced; i++)
		bcopy(&V_shmsegs[i], &newsegs[i], sizeof(newsegs[0]));
	for (; i < V_shminfo.shmmni; i++) {
		V_shmsegs[i].u.shm_perm.mode = SHMSEG_FREE;
		V_shmsegs[i].u.shm_perm.seq = 0;
#ifdef MAC
		mac_sysvshm_init(&V_shmsegs[i]);
#endif
	}
	free(V_shmsegs, M_SHM);
	V_shmsegs = newsegs;
	V_shmalloced = V_shminfo.shmmni;
}

static struct syscall_helper_data shm_syscalls[] = {
	SYSCALL_INIT_HELPER(shmat),
	SYSCALL_INIT_HELPER(shmctl),
	SYSCALL_INIT_HELPER(shmdt),
	SYSCALL_INIT_HELPER(shmget),
#if defined(COMPAT_FREEBSD4) || defined(COMPAT_FREEBSD5) || \
    defined(COMPAT_FREEBSD6) || defined(COMPAT_FREEBSD7)
	SYSCALL_INIT_HELPER_COMPAT(freebsd7_shmctl),
#endif
#if defined(__i386__) && (defined(COMPAT_FREEBSD4) || defined(COMPAT_43))
	SYSCALL_INIT_HELPER(shmsys),
#endif
	SYSCALL_INIT_LAST
};

#ifdef COMPAT_FREEBSD32
#include <compat/freebsd32/freebsd32.h>
#include <compat/freebsd32/freebsd32_ipc.h>
#include <compat/freebsd32/freebsd32_proto.h>
#include <compat/freebsd32/freebsd32_signal.h>
#include <compat/freebsd32/freebsd32_syscall.h>
#include <compat/freebsd32/freebsd32_util.h>

static struct syscall_helper_data shm32_syscalls[] = {
	SYSCALL32_INIT_HELPER_COMPAT(shmat),
	SYSCALL32_INIT_HELPER_COMPAT(shmdt),
	SYSCALL32_INIT_HELPER_COMPAT(shmget),
	SYSCALL32_INIT_HELPER(freebsd32_shmsys),
	SYSCALL32_INIT_HELPER(freebsd32_shmctl),
#if defined(COMPAT_FREEBSD4) || defined(COMPAT_FREEBSD5) || \
    defined(COMPAT_FREEBSD6) || defined(COMPAT_FREEBSD7)
	SYSCALL32_INIT_HELPER(freebsd7_freebsd32_shmctl),
#endif
	SYSCALL_INIT_LAST
};
#endif

#ifdef VPS

int shm_snapshot_vps(struct vps_snapst_ctx *ctx, struct vps *vps);
int shm_snapshot_proc(struct vps_snapst_ctx *ctx, struct vps *vps, struct proc* proc);
int shm_restore_vps(struct vps_snapst_ctx *ctx, struct vps *vps);
int shm_restore_proc(struct vps_snapst_ctx *ctx, struct vps *vps, struct proc* proc);
int shm_restore_fixup(struct vps_snapst_ctx *ctx, struct vps *vps);

static void
shm_vpsalloc_hook(void *arg, struct vps *vps)
{
	/*
	DPRINTF(("%s: vps=%p\n", __func__, vps));
	*/

	vps_ref(vps, NULL);

	shminit();
}

static void
shm_vpsfree_hook(void *arg, struct vps *vps)
{
	/*
	DPRINTF(("%s: vps=%p\n", __func__, vps));
	*/

	if (shmunload())
		printf("%s: shmunload() error\n", __func__);

	vps_deref(vps, NULL);
}

static int
shminit_global(void)
{
	struct vps *vps, *save_vps;
	int error;

	save_vps = curthread->td_vps;

	shm_nused_global = 0;

	sx_slock(&vps_all_lock);
	LIST_FOREACH(vps, &vps_head, vps_all) {
		curthread->td_vps = vps;
		shminit();
		curthread->td_vps = save_vps;
	}
	sx_sunlock(&vps_all_lock);

	shmexit_hook = &shmexit_myhook;
	shmfork_hook = &shmfork_myhook;

	shm_vpsalloc_tag = EVENTHANDLER_REGISTER(vps_alloc, shm_vpsalloc_hook, NULL,
		EVENTHANDLER_PRI_ANY);
	shm_vpsfree_tag = EVENTHANDLER_REGISTER(vps_free, shm_vpsfree_hook, NULL,
		EVENTHANDLER_PRI_ANY);

	vps_func->shm_snapshot_vps = shm_snapshot_vps;
	vps_func->shm_snapshot_proc = shm_snapshot_proc;
	vps_func->shm_restore_vps = shm_restore_vps;
	vps_func->shm_restore_proc = shm_restore_proc;
	vps_func->shm_restore_fixup = shm_restore_fixup;

	error = syscall_helper_register(shm_syscalls);
	if (error != 0)
		return (error);
#ifdef COMPAT_FREEBSD32
	error = syscall32_helper_register(shm32_syscalls);
	if (error != 0)
		return (error);
#endif
	return (error);
}

static int
shmunload_global(void)
{
	struct vps *vps, *save_vps;

	save_vps = curthread->td_vps;
 
	if (shm_nused_global != 0)
		return (EBUSY);

#ifdef COMPAT_FREEBSD32
	syscall32_helper_unregister(shm32_syscalls);
#endif
	syscall_helper_unregister(shm_syscalls);

	vps_func->shm_snapshot_vps = NULL;
	vps_func->shm_snapshot_proc = NULL;
	vps_func->shm_restore_vps = NULL;
	vps_func->shm_restore_proc = NULL;
	vps_func->shm_restore_fixup = NULL;

	EVENTHANDLER_DEREGISTER(vps_alloc, shm_vpsalloc_tag);
	EVENTHANDLER_DEREGISTER(vps_free, shm_vpsfree_tag);

	shmexit_hook = NULL;
	shmfork_hook = NULL;

	sx_slock(&vps_all_lock);
	LIST_FOREACH(vps, &vps_head, vps_all) {
		curthread->td_vps = vps;
		if (VPS_VPS(vps, shmsegs))
			shmunload();
		curthread->td_vps = save_vps;
	}
	sx_sunlock(&vps_all_lock);

	return (0);
}
#endif /* VPS */

static int
shminit()
{
	int i;
#ifndef VPS
	int error;
#endif

	V_shminfo.shmmax = SHMMAX;
	V_shminfo.shmmin = SHMMIN;
	V_shminfo.shmmni = SHMMNI;
	V_shminfo.shmseg = SHMSEG;
	V_shminfo.shmall = SHMALL;

#ifndef BURN_BRIDGES
	if (TUNABLE_ULONG_FETCH("kern.ipc.shmmaxpgs", &V_shminfo.shmall) != 0)
		printf("kern.ipc.shmmaxpgs is now called kern.ipc.shmall!\n");
#endif
	TUNABLE_ULONG_FETCH("kern.ipc.shmall", &V_shminfo.shmall);
		if (!TUNABLE_ULONG_FETCH("kern.ipc.shmmax", &V_shminfo.shmmax)) {
		/* Initialize shmmax dealing with possible overflow. */
		for (i = PAGE_SIZE; i > 0; i--) {
			V_shminfo.shmmax = V_shminfo.shmall * i;
			if (V_shminfo.shmmax >= V_shminfo.shmall)
				break;
		}
	}
	TUNABLE_ULONG_FETCH("kern.ipc.shmmin", &V_shminfo.shmmin);
	TUNABLE_ULONG_FETCH("kern.ipc.shmmni", &V_shminfo.shmmni);
	TUNABLE_ULONG_FETCH("kern.ipc.shmseg", &V_shminfo.shmseg);
	TUNABLE_INT_FETCH("kern.ipc.shm_use_phys", &V_shm_use_phys);

	V_shmalloced = V_shminfo.shmmni;
	V_shmsegs = malloc(V_shmalloced * sizeof(V_shmsegs[0]), M_SHM, M_WAITOK);
	for (i = 0; i < V_shmalloced; i++) {
		V_shmsegs[i].u.shm_perm.mode = SHMSEG_FREE;
		V_shmsegs[i].u.shm_perm.seq = 0;
		V_shmsegs[i].cred = NULL;
#ifdef MAC
		mac_sysvshm_init(&V_shmsegs[i]);
#endif
	}
	V_shm_last_free = 0;
	V_shm_nused = 0;
	V_shm_committed = 0;
#ifndef VPS
	shmexit_hook = &shmexit_myhook;
	shmfork_hook = &shmfork_myhook;

	error = syscall_helper_register(shm_syscalls);
	if (error != 0)
		return (error);
#ifdef COMPAT_FREEBSD32
	error = syscall32_helper_register(shm32_syscalls);
	if (error != 0)
		return (error);
#endif
#endif /* !VPS */
	return (0);
}

static int
shmunload()
{
	int i;	

#ifdef VPS
	/* Cleaning up */
	mtx_lock(&Giant);
	for (i = 0; i < V_shmalloced; i++)
		if ((V_shmsegs[i].u.shm_perm.mode & SHMSEG_ALLOCATED)
			&& V_shmsegs[i].object != NULL)
			shm_deallocate_segment(&V_shmsegs[i]);
	mtx_unlock(&Giant);
#endif
	if (V_shm_nused > 0)
		return (EBUSY);

#ifndef VPS
#ifdef COMPAT_FREEBSD32
	syscall32_helper_unregister(shm32_syscalls);
#endif
	syscall_helper_unregister(shm_syscalls);
#endif /* !VPS */

	for (i = 0; i < V_shmalloced; i++) {
#ifdef MAC
		mac_sysvshm_destroy(&V_shmsegs[i]);
#endif
		/*
		 * Objects might be still mapped into the processes
		 * address spaces.  Actual free would happen on the
		 * last mapping destruction.
		 */
		if (V_shmsegs[i].u.shm_perm.mode != SHMSEG_FREE)
			vm_object_deallocate(V_shmsegs[i].object);
	}
	free(V_shmsegs, M_SHM);
#ifndef VPS
	shmexit_hook = NULL;
	shmfork_hook = NULL;
#endif
	return (0);
}

static int
sysctl_shmsegs(SYSCTL_HANDLER_ARGS)
{

	return (SYSCTL_OUT(req, V_shmsegs, V_shmalloced * sizeof(V_shmsegs[0])));
}

#if defined(__i386__) && (defined(COMPAT_FREEBSD4) || defined(COMPAT_43))
struct oshmid_ds {
	struct	ipc_perm_old shm_perm;	/* operation perms */
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

static int
oshmctl(struct thread *td, struct oshmctl_args *uap)
{
#ifdef COMPAT_43
	int error = 0;
	struct shmid_kernel *shmseg;
	struct oshmid_ds outbuf;

	if (!prison_allow(td->td_ucred, PR_ALLOW_SYSVIPC))
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
		error = mac_sysvshm_check_shmctl(td->td_ucred, shmseg, uap->cmd);
		if (error != 0)
			goto done2;
#endif
		ipcperm_new2old(&shmseg->u.shm_perm, &outbuf.shm_perm);
		outbuf.shm_segsz = shmseg->u.shm_segsz;
		outbuf.shm_cpid = shmseg->u.shm_cpid;
		outbuf.shm_lpid = shmseg->u.shm_lpid;
		outbuf.shm_nattch = shmseg->u.shm_nattch;
		outbuf.shm_atime = shmseg->u.shm_atime;
		outbuf.shm_dtime = shmseg->u.shm_dtime;
		outbuf.shm_ctime = shmseg->u.shm_ctime;
		outbuf.shm_handle = shmseg->object;
		error = copyout(&outbuf, uap->ubuf, sizeof(outbuf));
		if (error)
			goto done2;
		break;
	default:
		error = freebsd7_shmctl(td, (struct freebsd7_shmctl_args *)uap);
		break;
	}
done2:
	mtx_unlock(&Giant);
	return (error);
#else
	return (EINVAL);
#endif
}

/* XXX casting to (sy_call_t *) is bogus, as usual. */
static sy_call_t *shmcalls[] = {
	(sy_call_t *)sys_shmat, (sy_call_t *)oshmctl,
	(sy_call_t *)sys_shmdt, (sy_call_t *)sys_shmget,
	(sy_call_t *)freebsd7_shmctl
};

int
sys_shmsys(td, uap)
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

	if (!prison_allow(td->td_ucred, PR_ALLOW_SYSVIPC))
		return (ENOSYS);
	if (uap->which < 0 ||
	    uap->which >= sizeof(shmcalls)/sizeof(shmcalls[0]))
		return (EINVAL);
	mtx_lock(&Giant);
	error = (*shmcalls[uap->which])(td, &uap->a2);
	mtx_unlock(&Giant);
	return (error);
}

#endif	/* i386 && (COMPAT_FREEBSD4 || COMPAT_43) */

#ifdef COMPAT_FREEBSD32

int
freebsd32_shmsys(struct thread *td, struct freebsd32_shmsys_args *uap)
{

#if defined(COMPAT_FREEBSD4) || defined(COMPAT_FREEBSD5) || \
    defined(COMPAT_FREEBSD6) || defined(COMPAT_FREEBSD7)
	switch (uap->which) {
	case 0:	{	/* shmat */
		struct shmat_args ap;

		ap.shmid = uap->a2;
		ap.shmaddr = PTRIN(uap->a3);
		ap.shmflg = uap->a4;
		return (sysent[SYS_shmat].sy_call(td, &ap));
	}
	case 2: {	/* shmdt */
		struct shmdt_args ap;

		ap.shmaddr = PTRIN(uap->a2);
		return (sysent[SYS_shmdt].sy_call(td, &ap));
	}
	case 3: {	/* shmget */
		struct shmget_args ap;

		ap.key = uap->a2;
		ap.size = uap->a3;
		ap.shmflg = uap->a4;
		return (sysent[SYS_shmget].sy_call(td, &ap));
	}
	case 4: {	/* shmctl */
		struct freebsd7_freebsd32_shmctl_args ap;

		ap.shmid = uap->a2;
		ap.cmd = uap->a3;
		ap.buf = PTRIN(uap->a4);
		return (freebsd7_freebsd32_shmctl(td, &ap));
	}
	case 1:		/* oshmctl */
	default:
		return (EINVAL);
	}
#else
	return (nosys(td, NULL));
#endif
}

#if defined(COMPAT_FREEBSD4) || defined(COMPAT_FREEBSD5) || \
    defined(COMPAT_FREEBSD6) || defined(COMPAT_FREEBSD7)
int
freebsd7_freebsd32_shmctl(struct thread *td,
    struct freebsd7_freebsd32_shmctl_args *uap)
{
	int error = 0;
	union {
		struct shmid_ds shmid_ds;
		struct shm_info shm_info;
		struct shminfo shminfo;
	} u;
	union {
		struct shmid_ds32_old shmid_ds32;
		struct shm_info32 shm_info32;
		struct shminfo32 shminfo32;
	} u32;
	size_t sz;

	if (uap->cmd == IPC_SET) {
		if ((error = copyin(uap->buf, &u32.shmid_ds32,
		    sizeof(u32.shmid_ds32))))
			goto done;
		freebsd32_ipcperm_old_in(&u32.shmid_ds32.shm_perm,
		    &u.shmid_ds.shm_perm);
		CP(u32.shmid_ds32, u.shmid_ds, shm_segsz);
		CP(u32.shmid_ds32, u.shmid_ds, shm_lpid);
		CP(u32.shmid_ds32, u.shmid_ds, shm_cpid);
		CP(u32.shmid_ds32, u.shmid_ds, shm_nattch);
		CP(u32.shmid_ds32, u.shmid_ds, shm_atime);
		CP(u32.shmid_ds32, u.shmid_ds, shm_dtime);
		CP(u32.shmid_ds32, u.shmid_ds, shm_ctime);
	}
	
	error = kern_shmctl(td, uap->shmid, uap->cmd, (void *)&u, &sz);
	if (error)
		goto done;
	
	/* Cases in which we need to copyout */
	switch (uap->cmd) {
	case IPC_INFO:
		CP(u.shminfo, u32.shminfo32, shmmax);
		CP(u.shminfo, u32.shminfo32, shmmin);
		CP(u.shminfo, u32.shminfo32, shmmni);
		CP(u.shminfo, u32.shminfo32, shmseg);
		CP(u.shminfo, u32.shminfo32, shmall);
		error = copyout(&u32.shminfo32, uap->buf,
		    sizeof(u32.shminfo32));
		break;
	case SHM_INFO:
		CP(u.shm_info, u32.shm_info32, used_ids);
		CP(u.shm_info, u32.shm_info32, shm_rss);
		CP(u.shm_info, u32.shm_info32, shm_tot);
		CP(u.shm_info, u32.shm_info32, shm_swp);
		CP(u.shm_info, u32.shm_info32, swap_attempts);
		CP(u.shm_info, u32.shm_info32, swap_successes);
		error = copyout(&u32.shm_info32, uap->buf,
		    sizeof(u32.shm_info32));
		break;
	case SHM_STAT:
	case IPC_STAT:
		freebsd32_ipcperm_old_out(&u.shmid_ds.shm_perm,
		    &u32.shmid_ds32.shm_perm);
		if (u.shmid_ds.shm_segsz > INT32_MAX)
			u32.shmid_ds32.shm_segsz = INT32_MAX;
		else
			CP(u.shmid_ds, u32.shmid_ds32, shm_segsz);
		CP(u.shmid_ds, u32.shmid_ds32, shm_lpid);
		CP(u.shmid_ds, u32.shmid_ds32, shm_cpid);
		CP(u.shmid_ds, u32.shmid_ds32, shm_nattch);
		CP(u.shmid_ds, u32.shmid_ds32, shm_atime);
		CP(u.shmid_ds, u32.shmid_ds32, shm_dtime);
		CP(u.shmid_ds, u32.shmid_ds32, shm_ctime);
		u32.shmid_ds32.shm_internal = 0;
		error = copyout(&u32.shmid_ds32, uap->buf,
		    sizeof(u32.shmid_ds32));
		break;
	}

done:
	if (error) {
		/* Invalidate the return value */
		td->td_retval[0] = -1;
	}
	return (error);
}
#endif

int
freebsd32_shmctl(struct thread *td, struct freebsd32_shmctl_args *uap)
{
	int error = 0;
	union {
		struct shmid_ds shmid_ds;
		struct shm_info shm_info;
		struct shminfo shminfo;
	} u;
	union {
		struct shmid_ds32 shmid_ds32;
		struct shm_info32 shm_info32;
		struct shminfo32 shminfo32;
	} u32;
	size_t sz;
	
	if (uap->cmd == IPC_SET) {
		if ((error = copyin(uap->buf, &u32.shmid_ds32,
		    sizeof(u32.shmid_ds32))))
			goto done;
		freebsd32_ipcperm_in(&u32.shmid_ds32.shm_perm,
		    &u.shmid_ds.shm_perm);
		CP(u32.shmid_ds32, u.shmid_ds, shm_segsz);
		CP(u32.shmid_ds32, u.shmid_ds, shm_lpid);
		CP(u32.shmid_ds32, u.shmid_ds, shm_cpid);
		CP(u32.shmid_ds32, u.shmid_ds, shm_nattch);
		CP(u32.shmid_ds32, u.shmid_ds, shm_atime);
		CP(u32.shmid_ds32, u.shmid_ds, shm_dtime);
		CP(u32.shmid_ds32, u.shmid_ds, shm_ctime);
	}
	
	error = kern_shmctl(td, uap->shmid, uap->cmd, (void *)&u, &sz);
	if (error)
		goto done;
	
	/* Cases in which we need to copyout */
	switch (uap->cmd) {
	case IPC_INFO:
		CP(u.shminfo, u32.shminfo32, shmmax);
		CP(u.shminfo, u32.shminfo32, shmmin);
		CP(u.shminfo, u32.shminfo32, shmmni);
		CP(u.shminfo, u32.shminfo32, shmseg);
		CP(u.shminfo, u32.shminfo32, shmall);
		error = copyout(&u32.shminfo32, uap->buf,
		    sizeof(u32.shminfo32));
		break;
	case SHM_INFO:
		CP(u.shm_info, u32.shm_info32, used_ids);
		CP(u.shm_info, u32.shm_info32, shm_rss);
		CP(u.shm_info, u32.shm_info32, shm_tot);
		CP(u.shm_info, u32.shm_info32, shm_swp);
		CP(u.shm_info, u32.shm_info32, swap_attempts);
		CP(u.shm_info, u32.shm_info32, swap_successes);
		error = copyout(&u32.shm_info32, uap->buf,
		    sizeof(u32.shm_info32));
		break;
	case SHM_STAT:
	case IPC_STAT:
		freebsd32_ipcperm_out(&u.shmid_ds.shm_perm,
		    &u32.shmid_ds32.shm_perm);
		if (u.shmid_ds.shm_segsz > INT32_MAX)
			u32.shmid_ds32.shm_segsz = INT32_MAX;
		else
			CP(u.shmid_ds, u32.shmid_ds32, shm_segsz);
		CP(u.shmid_ds, u32.shmid_ds32, shm_lpid);
		CP(u.shmid_ds, u32.shmid_ds32, shm_cpid);
		CP(u.shmid_ds, u32.shmid_ds32, shm_nattch);
		CP(u.shmid_ds, u32.shmid_ds32, shm_atime);
		CP(u.shmid_ds, u32.shmid_ds32, shm_dtime);
		CP(u.shmid_ds, u32.shmid_ds32, shm_ctime);
		error = copyout(&u32.shmid_ds32, uap->buf,
		    sizeof(u32.shmid_ds32));
		break;
	}

done:
	if (error) {
		/* Invalidate the return value */
		td->td_retval[0] = -1;
	}
	return (error);
}
#endif

#if defined(COMPAT_FREEBSD4) || defined(COMPAT_FREEBSD5) || \
    defined(COMPAT_FREEBSD6) || defined(COMPAT_FREEBSD7)

#ifndef CP
#define CP(src, dst, fld)	do { (dst).fld = (src).fld; } while (0)
#endif

#ifndef _SYS_SYSPROTO_H_
struct freebsd7_shmctl_args {
	int shmid;
	int cmd;
	struct shmid_ds_old *buf;
};
#endif
int
freebsd7_shmctl(td, uap)
	struct thread *td;
	struct freebsd7_shmctl_args *uap;
{
	int error = 0;
	struct shmid_ds_old old;
	struct shmid_ds buf;
	size_t bufsz;
	
	/*
	 * The only reason IPC_INFO, SHM_INFO, SHM_STAT exists is to support
	 * Linux binaries.  If we see the call come through the FreeBSD ABI,
	 * return an error back to the user since we do not to support this.
	 */
	if (uap->cmd == IPC_INFO || uap->cmd == SHM_INFO ||
	    uap->cmd == SHM_STAT)
		return (EINVAL);

	/* IPC_SET needs to copyin the buffer before calling kern_shmctl */
	if (uap->cmd == IPC_SET) {
		if ((error = copyin(uap->buf, &old, sizeof(old))))
			goto done;
		ipcperm_old2new(&old.shm_perm, &buf.shm_perm);
		CP(old, buf, shm_segsz);
		CP(old, buf, shm_lpid);
		CP(old, buf, shm_cpid);
		CP(old, buf, shm_nattch);
		CP(old, buf, shm_atime);
		CP(old, buf, shm_dtime);
		CP(old, buf, shm_ctime);
	}
	
	error = kern_shmctl(td, uap->shmid, uap->cmd, (void *)&buf, &bufsz);
	if (error)
		goto done;

	/* Cases in which we need to copyout */
	switch (uap->cmd) {
	case IPC_STAT:
		ipcperm_new2old(&buf.shm_perm, &old.shm_perm);
		if (buf.shm_segsz > INT_MAX)
			old.shm_segsz = INT_MAX;
		else
			CP(buf, old, shm_segsz);
		CP(buf, old, shm_lpid);
		CP(buf, old, shm_cpid);
		if (buf.shm_nattch > SHRT_MAX)
			old.shm_nattch = SHRT_MAX;
		else
			CP(buf, old, shm_nattch);
		CP(buf, old, shm_atime);
		CP(buf, old, shm_dtime);
		CP(buf, old, shm_ctime);
		old.shm_internal = NULL;
		error = copyout(&old, uap->buf, sizeof(old));
		break;
	}

done:
	if (error) {
		/* Invalidate the return value */
		td->td_retval[0] = -1;
	}
	return (error);
}

#endif	/* COMPAT_FREEBSD4 || COMPAT_FREEBSD5 || COMPAT_FREEBSD6 ||
	   COMPAT_FREEBSD7 */

static int
sysvshm_modload(struct module *module, int cmd, void *arg)
{
	int error = 0;

	switch (cmd) {
	case MOD_LOAD:
#ifdef VPS
		error = shminit_global();
		if (error != 0)
			shmunload();
#else
		error = shminit();
		if (error != 0)
			shmunload();
#endif
		break;
	case MOD_UNLOAD:
#ifdef VPS
		error = shmunload_global();
#else
		error = shmunload();
#endif
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

DECLARE_MODULE(sysvshm, sysvshm_mod, SI_SUB_SYSV_SHM, SI_ORDER_FIRST);
MODULE_VERSION(sysvshm, 1);

#ifdef VPS

__attribute__ ((noinline, unused))
int
shm_snapshot_vps(struct vps_snapst_ctx *ctx, struct vps *vps)
{
	struct vps_dump_sysvshm_shminfo *vdshminfo;
	struct vps_dump_sysvshm_shmid *vdshmsegs;
	struct vps_dumpobj *o1;
	struct shminfo *shminfo;
	struct shmid_kernel *shmsegs;
	int i;

	o1 = vdo_create(ctx, VPS_DUMPOBJT_SYSVSHM_VPS, M_WAITOK);

	shminfo = &VPS_VPS(vps, shminfo);
	vdshminfo = vdo_space(ctx, sizeof(*vdshminfo), M_WAITOK);

	vdshminfo->shmmax = shminfo->shmmax;
	vdshminfo->shmmin = shminfo->shmmin;
	vdshminfo->shmmni = shminfo->shmmni;
	vdshminfo->shmseg = shminfo->shmseg;
	vdshminfo->shmall = shminfo->shmall;
	vdshminfo->shm_last_free = VPS_VPS(vps, shm_last_free);
	vdshminfo->shm_nused = VPS_VPS(vps, shm_nused);
	vdshminfo->shmalloced = VPS_VPS(vps, shmalloced);
	vdshminfo->shm_committed = VPS_VPS(vps, shm_committed);

	shmsegs = VPS_VPS(vps, shmsegs);
	vdshmsegs = vdo_space(ctx, sizeof(*vdshmsegs) * shminfo->shmmni, M_WAITOK);
	for (i = 0; i < shminfo->shmmni; i++) {
		vdshmsegs[i].shm_perm.cuid = shmsegs[i].u.shm_perm.cuid;
		vdshmsegs[i].shm_perm.cgid = shmsegs[i].u.shm_perm.cgid;
		vdshmsegs[i].shm_perm.uid = shmsegs[i].u.shm_perm.uid;
		vdshmsegs[i].shm_perm.gid = shmsegs[i].u.shm_perm.gid;
		vdshmsegs[i].shm_perm.mode = shmsegs[i].u.shm_perm.mode;
		vdshmsegs[i].shm_perm.seq = shmsegs[i].u.shm_perm.seq;
		vdshmsegs[i].shm_perm.key = shmsegs[i].u.shm_perm.key;
		vdshmsegs[i].shm_segsz = shmsegs[i].u.shm_segsz;
		vdshmsegs[i].shm_lpid = shmsegs[i].u.shm_lpid;
		vdshmsegs[i].shm_cpid = shmsegs[i].u.shm_cpid;
		vdshmsegs[i].shm_nattch = shmsegs[i].u.shm_nattch;
		vdshmsegs[i].shm_atime = shmsegs[i].u.shm_atime;
		vdshmsegs[i].shm_ctime = shmsegs[i].u.shm_ctime;
		vdshmsegs[i].shm_dtime = shmsegs[i].u.shm_dtime;
		vdshmsegs[i].object = shmsegs[i].object;
		/* XXX assert label == NULL */
		vdshmsegs[i].label = shmsegs[i].label;
		vdshmsegs[i].cred = shmsegs[i].cred;
		if (shmsegs[i].cred != NULL)
			vps_func->vps_snapshot_ucred(ctx, vps, shmsegs[i].cred, M_WAITOK);
	}

	vdo_close(ctx);

	return (0);
}

__attribute__ ((noinline, unused))
int
shm_snapshot_proc(struct vps_snapst_ctx *ctx, struct vps *vps, struct proc* proc)
{
	struct vps_dumpobj *o1;
	struct vps_dump_sysvshm_shmmap_state *vdbase;
	struct shmmap_state *base;
	int i;

	if (proc->p_vmspace->vm_shm == NULL)
		return (0);

	o1 = vdo_create(ctx, VPS_DUMPOBJT_SYSVSHM_PROC, M_WAITOK);

	base = proc->p_vmspace->vm_shm;
	vdbase = vdo_space(ctx, sizeof(*vdbase) *
		VPS_VPS(vps, shminfo).shmseg, M_WAITOK);
	for (i = 0; i < VPS_VPS(vps, shminfo).shmseg; i++) {
		vdbase[i].va = base[i].va;
		vdbase[i].shmid = base[i].shmid;
	}

	vdo_close(ctx);

	return (0);
}

__attribute__ ((noinline, unused))
int
shm_restore_vps(struct vps_snapst_ctx *ctx, struct vps *vps)
{
	struct vps_dump_sysvshm_shminfo *vdshminfo;
	struct vps_dump_sysvshm_shmid *vdshmsegs;
	struct vps_dumpobj *o1;
	struct shminfo *shminfo;
	struct shmid_kernel *shmsegs;
	int i;

	o1 = vdo_next(ctx);
        if (o1->type != VPS_DUMPOBJT_SYSVSHM_VPS) {
		printf("%s: o1=%p is not VPS_DUMPOBJT_SYSVSHM_VPS\n",
			__func__, o1);
		return (EINVAL);
	}
	shminfo = &VPS_VPS(vps, shminfo);
	vdshminfo = (struct vps_dump_sysvshm_shminfo *)o1->data;

        shminfo->shmmax = vdshminfo->shmmax;
	shminfo->shmmin = vdshminfo->shmmin;
	shminfo->shmmni = vdshminfo->shmmni;
	shminfo->shmseg = vdshminfo->shmseg;
	shminfo->shmall = vdshminfo->shmall;

	free(VPS_VPS(vps, shmsegs), M_SHM);
	VPS_VPS(vps, shmsegs) = malloc(VPS_VPS(vps, shmalloced) *
		sizeof(VPS_VPS(vps, shmsegs[0])), M_SHM, M_WAITOK);

	VPS_VPS(vps, shm_last_free) = vdshminfo->shm_last_free;
	VPS_VPS(vps, shm_nused) = vdshminfo->shm_nused;
	VPS_VPS(vps, shmalloced) = vdshminfo->shmalloced;
	VPS_VPS(vps, shm_committed) = vdshminfo->shm_committed;

	shmsegs = VPS_VPS(vps, shmsegs);
	vdshmsegs = (struct vps_dump_sysvshm_shmid *)(vdshminfo + 1);
	for (i = 0; i < shminfo->shmmni; i++) {
		shmsegs[i].u.shm_perm.cuid = vdshmsegs[i].shm_perm.cuid;
		shmsegs[i].u.shm_perm.cgid = vdshmsegs[i].shm_perm.cgid;
		shmsegs[i].u.shm_perm.uid = vdshmsegs[i].shm_perm.uid;
		shmsegs[i].u.shm_perm.gid = vdshmsegs[i].shm_perm.gid;
		shmsegs[i].u.shm_perm.mode = vdshmsegs[i].shm_perm.mode;
		shmsegs[i].u.shm_perm.seq = vdshmsegs[i].shm_perm.seq;
		shmsegs[i].u.shm_perm.key = vdshmsegs[i].shm_perm.key;
		shmsegs[i].u.shm_segsz = vdshmsegs[i].shm_segsz;
		shmsegs[i].u.shm_lpid = vdshmsegs[i].shm_lpid;
		shmsegs[i].u.shm_cpid = vdshmsegs[i].shm_cpid;
		shmsegs[i].u.shm_nattch = vdshmsegs[i].shm_nattch;
		shmsegs[i].u.shm_atime = vdshmsegs[i].shm_atime;
		shmsegs[i].u.shm_ctime = vdshmsegs[i].shm_ctime;
		shmsegs[i].u.shm_dtime = vdshmsegs[i].shm_dtime;
		/* object fixed up later */
		shmsegs[i].object = vdshmsegs[i].object;
		/* XXX assert label == NULL */
		//shmsegs[i].label = vdshmsegs[i].label;
		shmsegs[i].label = NULL;
		shmsegs[i].cred = vdshmsegs[i].cred;
	}

	while (vdo_typeofnext(ctx) == VPS_DUMPOBJT_UCRED)
		vdo_next(ctx);//vps_func->vps_restore_ucred(ctx, vps);

	for (i = 0; i < shminfo->shmmni; i++) {
		if (shmsegs[i].cred != NULL)
			shmsegs[i].cred = vps_func->vps_restore_ucred_lookup(ctx, vps,
				shmsegs[i].cred);
	}

	return (0);
}

__attribute__ ((noinline, unused))
int
shm_restore_proc(struct vps_snapst_ctx *ctx, struct vps *vps, struct proc *proc)
{
	struct vps_dumpobj *o1;
	struct vps_dump_sysvshm_shmmap_state *vdbase;
	struct shmmap_state *base;
	int i;

	o1 = vdo_next(ctx);
        if (o1->type != VPS_DUMPOBJT_SYSVSHM_PROC) {
		printf("%s: o1=%p is not VPS_DUMPOBJT_SYSVSHM_PROC\n",
			__func__, o1);
		return (EINVAL);
	}

	proc->p_vmspace->vm_shm = malloc(sizeof(*base) * VPS_VPS(vps, shminfo).shmseg,
			M_SHM, M_WAITOK);
	base = proc->p_vmspace->vm_shm;
	vdbase = (struct vps_dump_sysvshm_shmmap_state *)o1->data;

	for (i = 0; i < VPS_VPS(vps, shminfo).shmseg; i++) {
		base[i].va = vdbase[i].va;
		base[i].shmid = vdbase[i].shmid;
	}

	return (0);
}

__attribute__ ((noinline, unused))
int
shm_restore_fixup(struct vps_snapst_ctx *ctx, struct vps *vps)
{
	struct vps_restore_obj *vbo;
	struct shmid_kernel *shmseg;
	int found;
	int i;

	for (i = 0; i < VPS_VPS(vps, shminfo).shmmni; i++) {
		shmseg = &VPS_VPS(vps, shmsegs)[i];

		if ( ! (shmseg->u.shm_perm.mode & SHMSEG_ALLOCATED) )
			continue;

		/* Look up vm object. */
		found = 0;
		SLIST_FOREACH(vbo, &ctx->obj_list, list)
			if (vbo->type == VPS_DUMPOBJT_VMOBJECT &&
			    vbo->orig_ptr == shmseg->object) {
				vm_object_reference(vbo->new_ptr);
				shmseg->object = vbo->new_ptr;
				found = 1;
				break;
			}
		KASSERT((found != 0), ("%s: object not found !\n", __func__));

		printf("%s: shmseg=%p i=%d object=%p, mode=%08x seq=%08x shm_nattch=%d\n",
			__func__, shmseg, i, shmseg->object, shmseg->u.shm_perm.mode,
			shmseg->u.shm_perm.seq, shmseg->u.shm_nattch);

	}

	return (0);
}
#endif

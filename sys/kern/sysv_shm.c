/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department. Originally from University of Wisconsin.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: Utah $Hdr: uipc_shm.c 1.9 89/08/14$
 *	from: @(#)sysv_shm.c	7.15 (Berkeley) 5/13/91
 *	$Id: sysv_shm.c,v 1.9 1994/01/21 09:56:31 davidg Exp $
 */

/*
 * System V shared memory routines.
 * TEMPORARY, until mmap is in place;
 * needed now for HP-UX compatibility and X server (yech!).
 */

#ifdef SYSVSHM

#include "param.h"
#include "systm.h"
#include "kernel.h"
#include "proc.h"
#include "shm.h"
#include "malloc.h"
#include "mman.h"
#include "vm/vm.h"
#include "vm/vm_kern.h"
#include "vm/vm_inherit.h"
#include "vm/vm_pager.h"
#include "vm/vm_user.h"

#ifdef HPUXCOMPAT
#include "hp300/hpux/hpux.h"
#endif

/* From shm.h */
struct	shmid_ds	*shmsegs;
struct	shminfo		shminfo;

int	shmat(), shmctl(), shmdt(), shmget(); /* XXX */
int	(*shmcalls[])() = { shmat, shmctl, shmdt, shmget }; /* XXX */
int	shmtot = 0;

/*
 * Per process internal structure for managing segments.
 * Each process using shm will have an array of ``shmseg'' of these.
 */
struct	shmdesc {
	vm_offset_t	shmd_uva;
	int		shmd_id;
};

/*
 * Per segment internal structure (shm_handle).
 */
struct	shmhandle {
	vm_offset_t	shmh_kva;
	caddr_t		shmh_id;
};

static int ipcaccess(struct ipc_perm *, int, struct ucred *);
static void shmufree(struct proc *, struct shmdesc *);
static void shmfree(struct shmid_ds *);
static int shmvalid(int);


vm_map_t shm_map;	/* address space for shared memory segments */

void
shminit()
{
	register int i;
	vm_offset_t whocares1, whocares2;

	shm_map = kmem_suballoc(kernel_map, &whocares1, &whocares2,
				shminfo.shmall * NBPG, FALSE);
	if (shminfo.shmmni > SHMMMNI)
		shminfo.shmmni = SHMMMNI;
	for (i = 0; i < shminfo.shmmni; i++) {
		shmsegs[i].shm_perm.mode = 0;
		shmsegs[i].shm_perm.seq = 0;
	}
}

TEXT_SET(pseudo_set, shminit);

/*
 * Entry point for all SHM calls
 */

struct shmsys_args {
	u_int which;
};

int
shmsys(p, uap, retval)
	struct proc *p;
	struct shmsys_args *uap;
	int *retval;
{

	if (uap->which >= sizeof(shmcalls)/sizeof(shmcalls[0]))
		return (EINVAL);
	return ((*shmcalls[uap->which])(p, &uap[1], retval));
}

/*
 * Get a shared memory segment
 */

struct shmget_args {
	key_t key;
	int size;
	int shmflg;
};

int
shmget(p, uap, retval)
	struct proc *p;
	register struct shmget_args *uap;
	int *retval;
{
	register struct shmid_ds *shp;
	register struct ucred *cred = p->p_ucred;
	register int i;
	int error, size, rval = 0;
	register struct shmhandle *shmh;

	/* look up the specified shm_id */
	if (uap->key != IPC_PRIVATE) {
		for (i = 0; i < shminfo.shmmni; i++)
			if ((shmsegs[i].shm_perm.mode & SHM_ALLOC) &&
			    shmsegs[i].shm_perm.key == uap->key) {
				rval = i;
				break;
			}
	} else
		i = shminfo.shmmni;

	/* create a new shared segment if necessary */
	if (i == shminfo.shmmni) {
		if ((uap->shmflg & IPC_CREAT) == 0)
			return (ENOENT);
		if (uap->size < shminfo.shmmin || uap->size > shminfo.shmmax)
			return (EINVAL);
		for (i = 0; i < shminfo.shmmni; i++)
			if ((shmsegs[i].shm_perm.mode & SHM_ALLOC) == 0) {
				rval = i;
				break;
			}
		if (i == shminfo.shmmni)
			return (ENOSPC);
		size = clrnd(btoc(uap->size));
		if (shmtot + size > shminfo.shmall)
			return (ENOMEM);
		shp = &shmsegs[rval];
		/*
		 * We need to do a couple of things to ensure consistency
		 * in case we sleep in malloc().  We mark segment as
		 * allocated so that other shmgets() will not allocate it.
		 * We mark it as "destroyed" to insure that shmvalid() is
		 * false making most operations fail (XXX).  We set the key,
		 * so that other shmget()s will fail.
		 */
		shp->shm_perm.mode = SHM_ALLOC | SHM_DEST;
		shp->shm_perm.key = uap->key;
		shmh = (struct shmhandle *)
			malloc(sizeof(struct shmhandle), M_SHM, M_WAITOK);
		shmh->shmh_kva = 0;
		shmh->shmh_id = (caddr_t)(0xc0000000UL|rval);	/* XXX */
		error = vm_mmap(shm_map, &shmh->shmh_kva, ctob(size),
				VM_PROT_ALL, VM_PROT_DEFAULT, MAP_ANON, shmh->shmh_id, 0);
		if (error) {
			free((caddr_t)shmh, M_SHM);
			shp->shm_perm.mode = 0;
			return(ENOMEM);
		}
		shp->shm_handle = (void *) shmh;
		shmtot += size;
		shp->shm_perm.cuid = shp->shm_perm.uid = cred->cr_uid;
		shp->shm_perm.cgid = shp->shm_perm.gid = cred->cr_gid;
		shp->shm_perm.mode = SHM_ALLOC | (uap->shmflg&0777);
		shp->shm_segsz = uap->size;
		shp->shm_cpid = p->p_pid;
		shp->shm_lpid = shp->shm_nattch = 0;
		shp->shm_atime = shp->shm_dtime = 0;
		shp->shm_ctime = time.tv_sec;
	} else {
		shp = &shmsegs[rval];
		/* XXX: probably not the right thing to do */
		if (shp->shm_perm.mode & SHM_DEST)
			return (EBUSY);
		if (error = ipcaccess(&shp->shm_perm, uap->shmflg&0777, cred))
			return (error);
		if (uap->size && uap->size > shp->shm_segsz)
			return (EINVAL);
		if ((uap->shmflg&IPC_CREAT) && (uap->shmflg&IPC_EXCL))
			return (EEXIST);
	}
	*retval = shp->shm_perm.seq * SHMMMNI + rval;
	return (0);
}

/*
 * Shared memory control
 */

struct shmctl_args {
	int shmid;
	int cmd;
	caddr_t buf;
};

/* ARGSUSED */
int
shmctl(p, uap, retval)
	struct proc *p;
	register struct shmctl_args *uap;
	int *retval;
{
	register struct shmid_ds *shp;
	register struct ucred *cred = p->p_ucred;
	struct shmid_ds sbuf;
	int error;

	if (error = shmvalid(uap->shmid))
		return (error);
	shp = &shmsegs[uap->shmid % SHMMMNI];
	switch (uap->cmd) {
	case IPC_STAT:
		if (error = ipcaccess(&shp->shm_perm, IPC_R, cred))
			return (error);
		return (copyout((caddr_t)shp, uap->buf, sizeof(*shp)));

	case IPC_SET:
		if (cred->cr_uid && cred->cr_uid != shp->shm_perm.uid &&
		    cred->cr_uid != shp->shm_perm.cuid)
			return (EPERM);
		if (error = copyin(uap->buf, (caddr_t)&sbuf, sizeof sbuf))
			return (error);
		shp->shm_perm.uid = sbuf.shm_perm.uid;
		shp->shm_perm.gid = sbuf.shm_perm.gid;
		shp->shm_perm.mode = (shp->shm_perm.mode & ~0777)
			| (sbuf.shm_perm.mode & 0777);
		shp->shm_ctime = time.tv_sec;
		break;

	case IPC_RMID:
		if (cred->cr_uid && cred->cr_uid != shp->shm_perm.uid &&
		    cred->cr_uid != shp->shm_perm.cuid)
			return (EPERM);
		/* set ctime? */
		shp->shm_perm.key = IPC_PRIVATE;
		shp->shm_perm.mode |= SHM_DEST;
		if (shp->shm_nattch <= 0)
			shmfree(shp);
		break;

#ifdef HPUXCOMPAT
	case SHM_LOCK:
	case SHM_UNLOCK:
		/* don't really do anything, but make them think we did */
		if ((p->p_flag & SHPUX) == 0)
			return (EINVAL);
		if (cred->cr_uid && cred->cr_uid != shp->shm_perm.uid &&
		    cred->cr_uid != shp->shm_perm.cuid)
			return (EPERM);
		break;
#endif

	default:
		return (EINVAL);
	}
	return (0);
}

/*
 * Attach to shared memory segment.
 */

struct shmat_args {
	int	shmid;
	caddr_t	shmaddr;
	int	shmflg;
};

int
shmat(p, uap, retval)
	struct proc *p;
	register struct shmat_args *uap;
	int *retval;
{
	register struct shmid_ds *shp;
	register int size;
	caddr_t uva;
	int error;
	int flags;
	vm_prot_t prot;
	struct shmdesc *shmd;

	/*
	 * Allocate descriptors now (before validity check)
	 * in case malloc() blocks.
	 */
	shmd = (struct shmdesc *)p->p_vmspace->vm_shm;
	size = shminfo.shmseg * sizeof(struct shmdesc);
	if (shmd == NULL) {
		shmd = (struct shmdesc *)malloc(size, M_SHM, M_WAITOK);
		bzero((caddr_t)shmd, size);
		p->p_vmspace->vm_shm = (caddr_t)shmd;
	}
	if (error = shmvalid(uap->shmid))
		return (error);
	shp = &shmsegs[uap->shmid % SHMMMNI];
	if (shp->shm_handle == NULL)
		panic("shmat NULL handle");
	if (error = ipcaccess(&shp->shm_perm,
	    (uap->shmflg&SHM_RDONLY) ? IPC_R : IPC_R|IPC_W, p->p_ucred))
		return (error);
	uva = uap->shmaddr;
	if (uva && ((int)uva & (SHMLBA-1))) {
		if (uap->shmflg & SHM_RND)
			uva = (caddr_t) ((int)uva & ~(SHMLBA-1));
		else
			return (EINVAL);
	}
	/*
	 * Make sure user doesn't use more than their fair share
	 */
	for (size = 0; size < shminfo.shmseg; size++) {
		if (shmd->shmd_uva == 0)
			break;
		shmd++;
	}
	if (size >= shminfo.shmseg)
		return (EMFILE);
	size = ctob(clrnd(btoc(shp->shm_segsz)));
	prot = VM_PROT_READ;
	if ((uap->shmflg & SHM_RDONLY) == 0)
		prot |= VM_PROT_WRITE;
	flags = MAP_ANON|MAP_SHARED;
	if (uva)
		flags |= MAP_FIXED;
	else
		uva = (caddr_t)0x1000000UL;	/* XXX */
	error = vm_mmap(&p->p_vmspace->vm_map, (vm_offset_t *)&uva,
			(vm_size_t)size, prot, VM_PROT_DEFAULT,
			flags, ((struct shmhandle *)shp->shm_handle)->shmh_id,
			0);
	if (error)
		return(error);
	shmd->shmd_uva = (vm_offset_t)uva;
	shmd->shmd_id = uap->shmid;
	/*
	 * Fill in the remaining fields
	 */
	shp->shm_lpid = p->p_pid;
	shp->shm_atime = time.tv_sec;
	shp->shm_nattch++;
	*retval = (int) uva;
	return (0);
}

/*
 * Detach from shared memory segment.
 */

struct shmdt_args {
	caddr_t	shmaddr;
};

/* ARGSUSED */
int
shmdt(p, uap, retval)
	struct proc *p;
	struct shmdt_args *uap;
	int *retval;
{
	register struct shmdesc *shmd;
	register int i;

	shmd = (struct shmdesc *)p->p_vmspace->vm_shm;
	for (i = 0; i < shminfo.shmseg; i++, shmd++)
		if (shmd->shmd_uva &&
		    shmd->shmd_uva == (vm_offset_t)uap->shmaddr)
			break;
	if (i == shminfo.shmseg)
		return(EINVAL);
	shmufree(p, shmd);
	shmsegs[shmd->shmd_id % SHMMMNI].shm_lpid = p->p_pid;
	return 0;
}

void
shmfork(p1, p2, isvfork)
	struct proc *p1, *p2;
	int isvfork;
{
	register struct shmdesc *shmd;
	register int size;

	/*
	 * Copy parents descriptive information
	 */
	size = shminfo.shmseg * sizeof(struct shmdesc);
	shmd = (struct shmdesc *)malloc(size, M_SHM, M_WAITOK);
	bcopy((caddr_t)p1->p_vmspace->vm_shm, (caddr_t)shmd, size);
	p2->p_vmspace->vm_shm = (caddr_t)shmd;
	/*
	 * Increment reference counts
	 */
	for (size = 0; size < shminfo.shmseg; size++, shmd++)
		if (shmd->shmd_uva)
			shmsegs[shmd->shmd_id % SHMMMNI].shm_nattch++;
}

void
shmexit(p)
	struct proc *p;
{
	register struct shmdesc *shmd;
	register int i;

	shmd = (struct shmdesc *)p->p_vmspace->vm_shm;
	for (i = 0; i < shminfo.shmseg; i++, shmd++)
		if (shmd->shmd_uva)
			shmufree(p, shmd);
	free((caddr_t)p->p_vmspace->vm_shm, M_SHM);
	p->p_vmspace->vm_shm = NULL;
}

static int
shmvalid(id)
	register int id;
{
	register struct shmid_ds *shp;

	if (id < 0 || (id % SHMMMNI) >= shminfo.shmmni)
		return(EINVAL);
	shp = &shmsegs[id % SHMMMNI];
	if (shp->shm_perm.seq == (id / SHMMMNI) &&
	    (shp->shm_perm.mode & (SHM_ALLOC|SHM_DEST)) == SHM_ALLOC)
		return(0);
	return(EINVAL);
}

/*
 * Free user resources associated with a shared memory segment
 */
static void
shmufree(p, shmd)
	struct proc *p;
	struct shmdesc *shmd;
{
	register struct shmid_ds *shp;

	shp = &shmsegs[shmd->shmd_id % SHMMMNI];
	(void) vm_deallocate(&p->p_vmspace->vm_map, shmd->shmd_uva,
			     ctob(clrnd(btoc(shp->shm_segsz))));
	shmd->shmd_id = 0;
	shmd->shmd_uva = 0;
	shp->shm_dtime = time.tv_sec;
	if (--shp->shm_nattch <= 0 && (shp->shm_perm.mode & SHM_DEST))
		shmfree(shp);
}

/*
 * Deallocate resources associated with a shared memory segment
 */
static void
shmfree(shp)
	register struct shmid_ds *shp;
{

	if (shp->shm_handle == NULL)
		panic("shmfree");
	/*
	 * Lose our lingering object reference by deallocating space
	 * in kernel.  Pager will also be deallocated as a side-effect.
	 */
	vm_deallocate(shm_map,
		      ((struct shmhandle *)shp->shm_handle)->shmh_kva,
		      ctob(clrnd(btoc(shp->shm_segsz))));
	free((caddr_t)shp->shm_handle, M_SHM);
	shp->shm_handle = NULL;
	shmtot -= clrnd(btoc(shp->shm_segsz));
	shp->shm_perm.mode = 0;
	/*
	 * Increment the sequence number to ensure that outstanding
	 * shmids for this segment will be invalid in the event that
	 * the segment is reallocated.  Note that shmids must be
	 * positive as decreed by SVID.
	 */
	shp->shm_perm.seq++;
	if ((int)(shp->shm_perm.seq * SHMMMNI) < 0)
		shp->shm_perm.seq = 0;
}
#endif /* SYSVSHM */

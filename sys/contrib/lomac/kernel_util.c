/*-
 * Copyright (c) 2001 Networks Associates Technology, Inc.
 * All rights reserved.
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/signalvar.h>
#include <sys/sx.h>
#include <sys/syscall.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/linker.h>
#include <sys/mount.h>
#include <sys/mman.h>
#include <sys/dirent.h>
#include <sys/namei.h>
#include <sys/uio.h>
#include <sys/unistd.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>

#include "kernel_interface.h"
#include "kernel_util.h"
#include "kernel_mediate.h"
#include "kernel_monitor.h"
#include "lomacfs.h"

#include "syscall_gate/syscall_gate.h"

#define	AS(name) (sizeof(struct name) / sizeof(register_t))

int
each_proc(int (*iter)(struct proc *p)) {
	struct proc *p;
	int error = 0;

	sx_slock(&allproc_lock);
	LIST_FOREACH(p, &allproc, p_list) {
		error = (*iter)(p);
		if (error)
			goto out;
	}
out:
	sx_sunlock(&allproc_lock);
	return (error);
}

static int
initialize_proc(struct proc *p) {
	lattr_t lattr = { LOMAC_HIGHEST_LEVEL, 0 };

	init_subject_lattr(p, &lattr);
	return (0);
}

static void
lomac_at_fork(struct proc *parent, struct proc *p, int flags) {

	if ((flags & RFMEM) == 0) {
		lattr_t parent_lattr;

		get_subject_lattr(parent, &parent_lattr);
		init_subject_lattr(p, &parent_lattr);
	}
}

static int
lomac_proc_candebug(struct proc *p1, struct proc *p2) {
	lattr_t lattr;

	get_subject_lattr(p1, &lattr);
	if (mediate_subject_level_subject("debug", p1, lattr.level, p2))
		return (0);
	else
		return (EPERM);
}

static int
lomac_proc_cansched(struct proc *p1, struct proc *p2) {
	lattr_t lattr;

	get_subject_lattr(p1, &lattr);
	if (mediate_subject_level_subject("sched", p1, lattr.level, p2))
		return (0);
	else
		return (EPERM);
}

static int
lomac_proc_cansignal(struct proc *p1, struct proc *p2, int signum) {
	lattr_t lattr;

	get_subject_lattr(p1, &lattr);
	/*
	 * Always allow signals to init(8) (necessary to shut down).
	 */
	if (p2->p_pid == 1 ||
	    mediate_subject_level_subject("signal", p1, lattr.level, p2))
		return (0);
	else
		return (EPERM);
}
	

int
lomac_initialize_procs(void) {
	int error;

#ifdef P_CAN_HOOKS
	can_hooks_lock();
	(void)p_candebug_hook(lomac_proc_candebug);
	(void)p_cansignal_hook(lomac_proc_cansignal);
	(void)p_cansched_hook(lomac_proc_cansched);
	can_hooks_unlock();
#endif
	error = at_fork(lomac_at_fork);
	if (error)
		return (error);
	return (each_proc(&initialize_proc));
}

int
lomac_uninitialize_procs(void) {

	rm_at_fork(lomac_at_fork);
	return (0);
}

extern int (*old_execve)(struct thread *, struct execve_args *);

int
execve(struct thread *td, struct execve_args *uap) {
	lattr_t lattr, textattr;
	struct vmspace *oldvmspace;
	struct proc *p;
	int error;

	p = td->td_proc;
	get_subject_lattr(p, &lattr);
	oldvmspace = p->p_vmspace;
	error = old_execve(td, uap);
	if (error == 0) {
		lomac_object_t lobj;

		lobj.lo_type = VISLOMAC(p->p_textvp) ? LO_TYPE_LVNODE :
		    LO_TYPE_UVNODE;
		lobj.lo_object.vnode = p->p_textvp;
		get_object_lattr(&lobj, &textattr);
		/*
		 * Install the executable's relevant attributes into the
		 * process.
		 */
		lattr.flags |= textattr.flags &
		    (LOMAC_ATTR_NODEMOTE | LOMAC_ATTR_NONETDEMOTE);
		if (p->p_vmspace != oldvmspace)
			init_subject_lattr(p, &lattr);
		else
			set_subject_lattr(p, lattr);
		mtx_lock(&Giant);
		(void)monitor_read_object(p, &lobj);
		mtx_unlock(&Giant);
	}
	return (error);
}

const char *linker_basename(const char* path);
int linker_load_module(const char *kldname, const char *modname,
	struct linker_file *parent, struct mod_depend *verinfo,
	struct linker_file **lfpp);

MALLOC_DECLARE(M_LINKER);

/*
 * MPSAFE
 */
int
kldload(struct thread* td, struct kldload_args* uap)
{
    char *kldname, *modname;
    char *pathname = NULL;
    linker_file_t lf;
    int error = 0;

    td->td_retval[0] = -1;

    if (securelevel > 0)	/* redundant, but that's OK */
	return EPERM;

    mtx_lock(&Giant);

    if ((error = suser_td(td)) != 0)
	goto out;

    pathname = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
    if ((error = copyinstr(SCARG(uap, file), pathname, MAXPATHLEN, NULL)) != 0)
	goto out;
    if (!mediate_subject_at_level("kldload", td->td_proc,
	LOMAC_HIGHEST_LEVEL)) {
	error = EPERM;
	goto out;
    }

    /*
     * If path do not contain qualified name or any dot in it (kldname.ko, or
     * kldname.ver.ko) treat it as interface name.
     */
    if (index(pathname, '/') || index(pathname, '.')) {
	kldname = pathname;
	modname = NULL;
    } else {
	kldname = NULL;
	modname = pathname;
    }
    error = linker_load_module(kldname, modname, NULL, NULL, &lf);
    if (error)
	goto out;

    lf->userrefs++;
    td->td_retval[0] = lf->id;

out:
    if (pathname)
	free(pathname, M_TEMP);
    mtx_unlock(&Giant);
    return (error);
}


#ifdef __i386__
#include <machine/sysarch.h>

extern int (*old_sysarch)(struct thread *, void *);

int
sysarch(struct thread *td, struct sysarch_args *uap) {
	switch (uap->op) {
	case I386_SET_IOPERM:
		if (!mediate_subject_at_level("ioperm", td->td_proc,
		    LOMAC_HIGHEST_LEVEL))
			return (EPERM);
	default:
		return (old_sysarch(td, uap));
	}
}
#endif

extern int lomac_mmap(struct proc *, struct mmap_args *);

/*
 * Mount a file system.
 */
#ifndef _SYS_SYSPROTO_H_
struct mount_args {
	char	*type;
	char	*path;
	int	flags;
	caddr_t	data;
};
#endif
/* ARGSUSED */
int
mount(td, uap)
	struct thread *td;
	struct mount_args /* {
		syscallarg(char *) type;
		syscallarg(char *) path;
		syscallarg(int) flags;
		syscallarg(caddr_t) data;
	} */ *uap;
{
	char *fstype;
	char *fspath;
	int error;

	fstype = malloc(MFSNAMELEN, M_TEMP, M_WAITOK | M_ZERO);
	fspath = malloc(MNAMELEN, M_TEMP, M_WAITOK | M_ZERO);

	/*
	 * vfs_mount() actually takes a kernel string for `type' and
	 * `path' now, so extract them.
	 */
	error = copyinstr(SCARG(uap, type), fstype, MFSNAMELEN, NULL);
	if (error)
		goto finish;
	error = copyinstr(SCARG(uap, path), fspath, MNAMELEN, NULL);
	if (error)
		goto finish;
	if (!mediate_subject_at_level("mount", td->td_proc,
	    LOMAC_HIGHEST_LEVEL)) {
		error = EPERM;
		goto finish;
	} 
	error = vfs_mount(td, fstype, fspath, SCARG(uap, flags),
	    SCARG(uap, data));
finish:
	free(fstype, M_TEMP);
	free(fspath, M_TEMP);
	return (error);
}

/*
 * Unmount a file system.
 *
 * Note: unmount takes a path to the vnode mounted on as argument,
 * not special file (as before).
 */
#ifndef _SYS_SYSPROTO_H_
struct unmount_args {
	char	*path;
	int	flags;
};
#endif
/* ARGSUSED */
int
unmount(td, uap)
	struct thread *td;
	register struct unmount_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
	} */ *uap;
{
	register struct vnode *vp;
	struct mount *mp;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), td);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;
	NDFREE(&nd, NDF_ONLY_PNBUF);
	mp = vp->v_mount;

	/*
	 * Only root, or the user that did the original mount is
	 * permitted to unmount this filesystem.
	 */
	if (!mediate_subject_at_level("unmount", td->td_proc,
	    LOMAC_HIGHEST_LEVEL) ||
	    ((mp->mnt_stat.f_owner != td->td_proc->p_ucred->cr_uid) &&
	    (error = suser_td(td)))) {
		vput(vp);
		return (error);
	}

	/*
	 * Don't allow unmounting the root file system.
	 */
	if (mp->mnt_flag & MNT_ROOTFS) {
		vput(vp);
		return (EINVAL);
	}

	/*
	 * Must be the root of the filesystem
	 */
	if ((vp->v_flag & VROOT) == 0) {
		vput(vp);
		return (EINVAL);
	}
	vput(vp);
	return (dounmount(mp, SCARG(uap, flags), td));
}

static struct syscall_override {
	int offset;
	sy_call_t *call;
	int narg;
	int mpsafe;
} syscall_overrides[] = {
	{ SYS_mmap, (sy_call_t *)mmap, AS(mmap_args), 1 },
	{ SYS_execve, (sy_call_t *)execve, AS(execve_args), 1 },
	{ SYS_kldload, (sy_call_t *)kldload, AS(kldload_args), 1 },
	{ SYS_mount, (sy_call_t *)mount, AS(mount_args), 0 },
	{ SYS_unmount, (sy_call_t *)unmount, AS(unmount_args), 0 },
#ifdef __i386__
	{ SYS_sysarch, (sy_call_t *)sysarch, AS(sysarch_args), 1 }
#endif
};

int
lomac_initialize_syscalls(void) {
	int error, i;

	for (i = 0;
	    i < sizeof(syscall_overrides) / sizeof(syscall_overrides[0]); i++) {
		struct syscall_override *so = &syscall_overrides[i];

		error = syscall_gate_register(so->offset, so->call, so->narg,
		    so->mpsafe);
		if (error) {
			while (--i >= 0)
				syscall_gate_deregister(
				    syscall_overrides[i].offset);
			return (error);
		}
	}
	return (0);
}

int
lomac_uninitialize_syscalls(void) {
	int i;

	for (i = 0;
	    i < sizeof(syscall_overrides) / sizeof(syscall_overrides[0]); i++)
		syscall_gate_deregister(syscall_overrides[i].offset);
	return (0);
}

/* This memory is shared by all lomac_do_recwd() calls, in sequence. */
static char *pathmem;
#define	DIRENTMEM_SIZE (64 << 10)	/* 64KB is good, I guess! */
static char *direntmem;

static int
lomac_dirents_searchbyid(struct vnode *dvp, struct dirent *dp,
    struct dirent *enddp, const struct vattr *vap, struct dirent **retdp)
{
	struct vattr pvattr;
	struct componentname cnp;
	struct thread *td = curthread;
	struct ucred *ucred = td->td_ucred;
	struct vnode *vp;
	int error;

	*retdp = NULL;
	for (; dp != enddp; dp = (struct dirent *)((char *)dp + dp->d_reclen)) {
		cnp.cn_nameiop = LOOKUP;
		cnp.cn_flags = LOCKPARENT | ISLASTCN | NOFOLLOW;
		cnp.cn_thread = td;
		cnp.cn_cred = ucred;
		cnp.cn_nameptr = dp->d_name;
		cnp.cn_namelen = dp->d_namlen;
		
		error = VOP_LOOKUP(dvp, &vp, &cnp);
		if (error)
			return (error);
		error = VOP_GETATTR(vp, &pvattr, ucred, td);
		if (vp != dvp)
			(void)vput(vp);
		else
			vrele(vp);	/* if looking up "." */
		if (error)
			return (error);
		if (pvattr.va_fsid == vap->va_fsid &&
		    pvattr.va_fileid == vap->va_fileid) {
			*retdp = dp;
			break;
		}
	}
	return (0);
}

static int
lomac_getcwd(
	struct thread *td,
	char *buf,
	size_t buflen,
	char **bufret
) {
	struct vattr cvattr;
	char *bp;
	int error, i, slash_prefixed;
	struct filedesc *fdp;
	struct vnode *vp, *startvp, *dvp;

	if (buflen < 2)
		return (EINVAL);
	if (buflen > MAXPATHLEN)
		buflen = MAXPATHLEN;
	bp = buf;
	bp += buflen - 1;
	*bp = '\0';
	fdp = td->td_proc->p_fd;
	slash_prefixed = 0;
#if defined(LOMAC_DEBUG_RECWD)
	printf("lomac_getcwd for %d:\n", td->td_proc->p_pid);
#endif
	startvp = fdp->fd_cdir;
	vref(startvp);
	for (vp = startvp; vp != rootvnode; vp = dvp) {
		struct iovec diov = {
			direntmem,
			DIRENTMEM_SIZE
		};
		struct uio duio = {
			&diov,
			1,
			0,
			DIRENTMEM_SIZE,
			UIO_SYSSPACE,
			UIO_READ,
			curthread
		};
		struct dirent *dp;
		int direof;

		if (vp->v_flag & VROOT) {
			if (vp->v_mount == NULL)	/* forced unmount */
				return (EBADF);
			dvp = vp->v_mount->mnt_vnodecovered;
			continue;
		}
		dvp = vp->v_dd;
		if (vp == dvp)
			break;
		/*
		 * Utilize POSIX requirement of files having same
		 * st_dev and st_ino to be the same  file, in our
		 * case with vattr.va_fsid and vattr.va_fileid.
		 */
		error = vget(vp, LK_EXCLUSIVE, curthread);
		if (error)
			goto out2;
		error = VOP_GETATTR(vp, &cvattr, curthread->td_ucred,
		    curthread);
		if (error)
			goto out2;
		(void)vput(vp);
		error = vget(dvp, LK_EXCLUSIVE, curthread);
		if (error)
			goto out2;
		for (direof = 0; !direof;) {
			error = VOP_READDIR(dvp, &duio,
			    curthread->td_ucred, &direof, NULL, NULL);
			if (error)
				break;
			error = lomac_dirents_searchbyid(dvp,
			    (struct dirent *)direntmem,
			    (struct dirent *)(direntmem +
			    DIRENTMEM_SIZE - duio.uio_resid),
			    &cvattr,
			    &dp);
			if (error)
				break;
			if (dp != NULL) {
				(void)vput(dvp);
#if defined(LOMAC_DEBUG_RECWD)
				printf("\tdirent component: \"%.*s\"\n",
				    dp->d_namlen, dp->d_name);
#endif
				for (i = dp->d_namlen - 1; i >= 0; i--)
					if (bp == buf)
						return (ENOMEM);
					else
						*--bp = dp->d_name[i];
				goto nextcomp;
			}
			diov.iov_base = direntmem;
			diov.iov_len = DIRENTMEM_SIZE;
			duio.uio_resid = DIRENTMEM_SIZE;
		}
		if (direof)
			error = ENOENT;
		(void)vput(dvp);
	out2:
#if defined(LOMAC_DEBUG_RECWD)
		printf("backup dirent lookup problem: %d\n", error);
#endif
		goto out;
	nextcomp:
		if (bp == buf)
			return (ENOMEM);
		*--bp = '/';
		slash_prefixed = 1;
	}
	if (!slash_prefixed) {
		if (bp == buf)
			return (ENOMEM);
		*--bp = '/';
	}
	error = 0;
	*bufret = bp;
out:
	vrele(startvp);
	return (error);
}

static int
lomac_do_recwd(struct proc *p) {
	struct nameidata nd;
	struct filedesc *fdp = curthread->td_proc->p_fd;
	struct thread *td = &p->p_thread;
	char *nbuf;
	struct vnode *cdir, *rdir, *vp;
	int error;
	
	if (p == curthread->td_proc)
		return (0);
	PROC_LOCK(p);
	if (p->p_flag & P_SYSTEM) {
		PROC_UNLOCK(p);
		return (0);
	}
	PROC_UNLOCK(p);
	error = lomac_getcwd(td, pathmem, MAXPATHLEN, &nbuf);
	if (error) {
#if defined(LOMAC_DEBUG_RECWD)
		printf("lomac: recwd() failure, lomac_getcwd() == %d\n",
		   error);
#endif
		return (0);
	}
	rdir = fdp->fd_rdir;
	fdp->fd_rdir = rootvnode;
	vref(fdp->fd_rdir);
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE,
	    nbuf, curthread);
	error = namei(&nd);
	vrele(fdp->fd_rdir);
	fdp->fd_rdir = rdir;
	if (error == 0) {
		vp = nd.ni_vp;
		if (vp->v_type != VDIR)
			error = ENOTDIR;
		else
			error = VOP_ACCESS(vp, VEXEC, td->td_proc->p_ucred,
			    curthread);
		if (error)
			vput(vp);
		else {
			NDFREE(&nd, NDF_ONLY_PNBUF);
			fdp = p->p_fd;
			cdir = fdp->fd_cdir;
			fdp->fd_cdir = vp;
			vrele(cdir);
			VOP_UNLOCK(vp, 0, curthread);
		}
	}
#if defined(LOMAC_DEBUG_RECWD)
	printf("\trecwd() to \"%.*s\" == %d\n",
	    MAXPATHLEN, nbuf, error);
#endif
	return (0);
}

int
lomac_initialize_cwds(void) {
	int error;

	pathmem = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
	direntmem = malloc(DIRENTMEM_SIZE, M_TEMP, M_WAITOK);
	mtx_lock(&Giant);
	error = each_proc(lomac_do_recwd);
	mtx_unlock(&Giant);
	free(pathmem, M_TEMP);
	free(direntmem, M_TEMP);
	return (error);
}

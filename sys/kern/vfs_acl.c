/*-
 * Copyright (c) 1999-2006 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
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
/*
 * Developed by the TrustedBSD Project.
 *
 * ACL system calls and other functions common across different ACL types.
 * Type-specific routines go into subr_acl_<type>.c.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/kern/vfs_acl.c,v 1.53.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/proc.h>
#include <sys/sysent.h>
#include <sys/acl.h>

#include <security/mac/mac_framework.h>

#include <vm/uma.h>

uma_zone_t	acl_zone;
static int	vacl_set_acl(struct thread *td, struct vnode *vp,
		    acl_type_t type, struct acl *aclp);
static int	vacl_get_acl(struct thread *td, struct vnode *vp,
		    acl_type_t type, struct acl *aclp);
static int	vacl_aclcheck(struct thread *td, struct vnode *vp,
		    acl_type_t type, struct acl *aclp);

/*
 * These calls wrap the real vnode operations, and are called by the syscall
 * code once the syscall has converted the path or file descriptor to a vnode
 * (unlocked).  The aclp pointer is assumed still to point to userland, so
 * this should not be consumed within the kernel except by syscall code.
 * Other code should directly invoke VOP_{SET,GET}ACL.
 */

/*
 * Given a vnode, set its ACL.
 */
static int
vacl_set_acl(struct thread *td, struct vnode *vp, acl_type_t type,
    struct acl *aclp)
{
	struct acl inkernacl;
	struct mount *mp;
	int error;

	error = copyin(aclp, &inkernacl, sizeof(struct acl));
	if (error)
		return(error);
	error = vn_start_write(vp, &mp, V_WAIT | PCATCH);
	if (error != 0)
		return (error);
	VOP_LEASE(vp, td, td->td_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
#ifdef MAC
	error = mac_check_vnode_setacl(td->td_ucred, vp, type, &inkernacl);
	if (error != 0)
		goto out;
#endif
	error = VOP_SETACL(vp, type, &inkernacl, td->td_ucred, td);
#ifdef MAC
out:
#endif
	VOP_UNLOCK(vp, 0, td);
	vn_finished_write(mp);
	return(error);
}

/*
 * Given a vnode, get its ACL.
 */
static int
vacl_get_acl(struct thread *td, struct vnode *vp, acl_type_t type,
    struct acl *aclp)
{
	struct acl inkernelacl;
	int error;

	VOP_LEASE(vp, td, td->td_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
#ifdef MAC
	error = mac_check_vnode_getacl(td->td_ucred, vp, type);
	if (error != 0)
		goto out;
#endif
	error = VOP_GETACL(vp, type, &inkernelacl, td->td_ucred, td);
#ifdef MAC
out:
#endif
	VOP_UNLOCK(vp, 0, td);
	if (error == 0)
		error = copyout(&inkernelacl, aclp, sizeof(struct acl));
	return (error);
}

/*
 * Given a vnode, delete its ACL.
 */
static int
vacl_delete(struct thread *td, struct vnode *vp, acl_type_t type)
{
	struct mount *mp;
	int error;

	error = vn_start_write(vp, &mp, V_WAIT | PCATCH);
	if (error)
		return (error);
	VOP_LEASE(vp, td, td->td_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
#ifdef MAC
	error = mac_check_vnode_deleteacl(td->td_ucred, vp, type);
	if (error)
		goto out;
#endif
	error = VOP_SETACL(vp, type, 0, td->td_ucred, td);
#ifdef MAC
out:
#endif
	VOP_UNLOCK(vp, 0, td);
	vn_finished_write(mp);
	return (error);
}

/*
 * Given a vnode, check whether an ACL is appropriate for it
 */
static int
vacl_aclcheck(struct thread *td, struct vnode *vp, acl_type_t type,
    struct acl *aclp)
{
	struct acl inkernelacl;
	int error;

	error = copyin(aclp, &inkernelacl, sizeof(struct acl));
	if (error)
		return(error);
	error = VOP_ACLCHECK(vp, type, &inkernelacl, td->td_ucred, td);
	return (error);
}

/*
 * syscalls -- convert the path/fd to a vnode, and call vacl_whatever.  Don't
 * need to lock, as the vacl_ code will get/release any locks required.
 */

/*
 * Given a file path, get an ACL for it
 */
int
__acl_get_file(struct thread *td, struct __acl_get_file_args *uap)
{
	struct nameidata nd;
	int vfslocked, error;

	NDINIT(&nd, LOOKUP, MPSAFE|FOLLOW, UIO_USERSPACE, uap->path, td);
	error = namei(&nd);
	vfslocked = NDHASGIANT(&nd);
	if (error == 0) {
		error = vacl_get_acl(td, nd.ni_vp, uap->type, uap->aclp);
		NDFREE(&nd, 0);
	}
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

/*
 * Given a file path, get an ACL for it; don't follow links.
 */
int
__acl_get_link(struct thread *td, struct __acl_get_link_args *uap)
{
	struct nameidata nd;
	int vfslocked, error;

	NDINIT(&nd, LOOKUP, MPSAFE|NOFOLLOW, UIO_USERSPACE, uap->path, td);
	error = namei(&nd);
	vfslocked = NDHASGIANT(&nd);
	if (error == 0) {
		error = vacl_get_acl(td, nd.ni_vp, uap->type, uap->aclp);
		NDFREE(&nd, 0);
	}
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

/*
 * Given a file path, set an ACL for it.
 */
int
__acl_set_file(struct thread *td, struct __acl_set_file_args *uap)
{
	struct nameidata nd;
	int vfslocked, error;

	NDINIT(&nd, LOOKUP, MPSAFE|FOLLOW, UIO_USERSPACE, uap->path, td);
	error = namei(&nd);
	vfslocked = NDHASGIANT(&nd);
	if (error == 0) {
		error = vacl_set_acl(td, nd.ni_vp, uap->type, uap->aclp);
		NDFREE(&nd, 0);
	}
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

/*
 * Given a file path, set an ACL for it; don't follow links.
 */
int
__acl_set_link(struct thread *td, struct __acl_set_link_args *uap)
{
	struct nameidata nd;
	int vfslocked, error;

	NDINIT(&nd, LOOKUP, MPSAFE|NOFOLLOW, UIO_USERSPACE, uap->path, td);
	error = namei(&nd);
	vfslocked = NDHASGIANT(&nd);
	if (error == 0) {
		error = vacl_set_acl(td, nd.ni_vp, uap->type, uap->aclp);
		NDFREE(&nd, 0);
	}
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

/*
 * Given a file descriptor, get an ACL for it.
 */
int
__acl_get_fd(struct thread *td, struct __acl_get_fd_args *uap)
{
	struct file *fp;
	int vfslocked, error;

	error = getvnode(td->td_proc->p_fd, uap->filedes, &fp);
	if (error == 0) {
		vfslocked = VFS_LOCK_GIANT(fp->f_vnode->v_mount);
		error = vacl_get_acl(td, fp->f_vnode, uap->type, uap->aclp);
		fdrop(fp, td);
		VFS_UNLOCK_GIANT(vfslocked);
	}
	return (error);
}

/*
 * Given a file descriptor, set an ACL for it.
 */
int
__acl_set_fd(struct thread *td, struct __acl_set_fd_args *uap)
{
	struct file *fp;
	int vfslocked, error;

	error = getvnode(td->td_proc->p_fd, uap->filedes, &fp);
	if (error == 0) {
		vfslocked = VFS_LOCK_GIANT(fp->f_vnode->v_mount);
		error = vacl_set_acl(td, fp->f_vnode, uap->type, uap->aclp);
		fdrop(fp, td);
		VFS_UNLOCK_GIANT(vfslocked);
	}
	return (error);
}

/*
 * Given a file path, delete an ACL from it.
 */
int
__acl_delete_file(struct thread *td, struct __acl_delete_file_args *uap)
{
	struct nameidata nd;
	int vfslocked, error;

	NDINIT(&nd, LOOKUP, MPSAFE|FOLLOW, UIO_USERSPACE, uap->path, td);
	error = namei(&nd);
	vfslocked = NDHASGIANT(&nd);
	if (error == 0) {
		error = vacl_delete(td, nd.ni_vp, uap->type);
		NDFREE(&nd, 0);
	}
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

/*
 * Given a file path, delete an ACL from it; don't follow links.
 */
int
__acl_delete_link(struct thread *td, struct __acl_delete_link_args *uap)
{
	struct nameidata nd;
	int vfslocked, error;

	NDINIT(&nd, LOOKUP, MPSAFE|NOFOLLOW, UIO_USERSPACE, uap->path, td);
	error = namei(&nd);
	vfslocked = NDHASGIANT(&nd);
	if (error == 0) {
		error = vacl_delete(td, nd.ni_vp, uap->type);
		NDFREE(&nd, 0);
	}
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

/*
 * Given a file path, delete an ACL from it.
 */
int
__acl_delete_fd(struct thread *td, struct __acl_delete_fd_args *uap)
{
	struct file *fp;
	int vfslocked, error;

	error = getvnode(td->td_proc->p_fd, uap->filedes, &fp);
	if (error == 0) {
		vfslocked = VFS_LOCK_GIANT(fp->f_vnode->v_mount);
		error = vacl_delete(td, fp->f_vnode, uap->type);
		fdrop(fp, td);
		VFS_UNLOCK_GIANT(vfslocked);
	}
	return (error);
}

/*
 * Given a file path, check an ACL for it.
 */
int
__acl_aclcheck_file(struct thread *td, struct __acl_aclcheck_file_args *uap)
{
	struct nameidata	nd;
	int vfslocked, error;

	NDINIT(&nd, LOOKUP, MPSAFE|FOLLOW, UIO_USERSPACE, uap->path, td);
	error = namei(&nd);
	vfslocked = NDHASGIANT(&nd);
	if (error == 0) {
		error = vacl_aclcheck(td, nd.ni_vp, uap->type, uap->aclp);
		NDFREE(&nd, 0);
	}
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

/*
 * Given a file path, check an ACL for it; don't follow links.
 */
int
__acl_aclcheck_link(struct thread *td, struct __acl_aclcheck_link_args *uap)
{
	struct nameidata	nd;
	int vfslocked, error;

	NDINIT(&nd, LOOKUP, MPSAFE|NOFOLLOW, UIO_USERSPACE, uap->path, td);
	error = namei(&nd);
	vfslocked = NDHASGIANT(&nd);
	if (error == 0) {
		error = vacl_aclcheck(td, nd.ni_vp, uap->type, uap->aclp);
		NDFREE(&nd, 0);
	}
	VFS_UNLOCK_GIANT(vfslocked);
	return (error);
}

/*
 * Given a file descriptor, check an ACL for it.
 */
int
__acl_aclcheck_fd(struct thread *td, struct __acl_aclcheck_fd_args *uap)
{
	struct file *fp;
	int vfslocked, error;

	error = getvnode(td->td_proc->p_fd, uap->filedes, &fp);
	if (error == 0) {
		vfslocked = VFS_LOCK_GIANT(fp->f_vnode->v_mount);
		error = vacl_aclcheck(td, fp->f_vnode, uap->type, uap->aclp);
		fdrop(fp, td);
		VFS_UNLOCK_GIANT(vfslocked);
	}
	return (error);
}

/* ARGUSED */

static void
aclinit(void *dummy __unused)
{

	acl_zone = uma_zcreate("ACL UMA zone", sizeof(struct acl),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
}
SYSINIT(acls, SI_SUB_ACL, SI_ORDER_FIRST, aclinit, NULL);

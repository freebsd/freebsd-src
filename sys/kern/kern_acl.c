/*-
 * Copyright (c) 1999, 2000 Robert N. M. Watson
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
 * $FreeBSD: src/sys/kern/kern_acl.c,v 1.2.2.1 2000/07/28 18:48:16 rwatson Exp $
 */

/*
 * Generic routines to support file system ACLs, at a syntactic level
 * Semantics are the responsibility of the underlying file system
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/lock.h>
#include <sys/namei.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/sysent.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/acl.h>

static MALLOC_DEFINE(M_ACL, "acl", "access control list");

static int	vacl_set_acl(struct proc *p, struct vnode *vp, acl_type_t type,
	    struct acl *aclp);
static int	vacl_get_acl(struct proc *p, struct vnode *vp, acl_type_t type,
	    struct acl *aclp);
static int	vacl_aclcheck(struct proc *p, struct vnode *vp, acl_type_t type,
	    struct acl *aclp);

/*
 * These calls wrap the real vnode operations, and are called by the 
 * syscall code once the syscall has converted the path or file
 * descriptor to a vnode (unlocked).  The aclp pointer is assumed
 * still to point to userland, so this should not be consumed within
 * the kernel except by syscall code.  Other code should directly
 * invoke VOP_{SET,GET}ACL.
 */

/*
 * Given a vnode, set its ACL.
 */
static int
vacl_set_acl(struct proc *p, struct vnode *vp, acl_type_t type,
    struct acl *aclp)
{
	struct acl inkernacl;
	int error;

	error = copyin(aclp, &inkernacl, sizeof(struct acl));
	if (error)
		return(error);
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	error = VOP_SETACL(vp, type, &inkernacl, p->p_ucred, p);
	VOP_UNLOCK(vp, 0, p);
	return(error);
}

/*
 * Given a vnode, get its ACL.
 */
static int
vacl_get_acl(struct proc *p, struct vnode *vp, acl_type_t type,
    struct acl *aclp)
{
	struct acl inkernelacl;
	int error;

	error = VOP_GETACL(vp, type, &inkernelacl, p->p_ucred, p);
	if (error == 0)
		error = copyout(&inkernelacl, aclp, sizeof(struct acl));
	return (error);
}

/*
 * Given a vnode, delete its ACL.
 */
static int
vacl_delete(struct proc *p, struct vnode *vp, acl_type_t type)
{
	int error;

	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	error = VOP_SETACL(vp, ACL_TYPE_DEFAULT, 0, p->p_ucred, p);
	VOP_UNLOCK(vp, 0, p);
	return (error);
}

/*
 * Given a vnode, check whether an ACL is appropriate for it
 */
static int
vacl_aclcheck(struct proc *p, struct vnode *vp, acl_type_t type,
    struct acl *aclp)
{
	struct acl inkernelacl;
	int error;

	error = copyin(aclp, &inkernelacl, sizeof(struct acl));
	if (error)
		return(error);
	error = VOP_ACLCHECK(vp, type, &inkernelacl, p->p_ucred, p);
	return (error);
}

/*
 * syscalls -- convert the path/fd to a vnode, and call vacl_whatever.
 * Don't need to lock, as the vacl_ code will get/release any locks
 * required.
 */

/*
 * Given a file path, get an ACL for it
 */
int
__acl_get_file(struct proc *p, struct __acl_get_file_args *uap)
{
	struct nameidata nd;
	int error;

	/* what flags are required here -- possible not LOCKLEAF? */
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	error = namei(&nd);
	if (error)
		return(error);
	error = vacl_get_acl(p, nd.ni_vp, SCARG(uap, type), SCARG(uap, aclp));
	NDFREE(&nd, 0);
	return (error);
}

/*
 * Given a file path, set an ACL for it
 */
int
__acl_set_file(struct proc *p, struct __acl_set_file_args *uap)
{
	struct nameidata nd;
	int error;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	error = namei(&nd);
	if (error)
		return(error);
	error = vacl_set_acl(p, nd.ni_vp, SCARG(uap, type), SCARG(uap, aclp));
	NDFREE(&nd, 0);
	return (error);
}

/*
 * Given a file descriptor, get an ACL for it
 */
int
__acl_get_fd(struct proc *p, struct __acl_get_fd_args *uap)
{
	struct file *fp;
	int error;

	error = getvnode(p->p_fd, SCARG(uap, filedes), &fp);
	if (error)
		return(error);
	return vacl_get_acl(p, (struct vnode *)fp->f_data, SCARG(uap, type),
	    SCARG(uap, aclp));
}

/*
 * Given a file descriptor, set an ACL for it
 */
int
__acl_set_fd(struct proc *p, struct __acl_set_fd_args *uap)
{
	struct file *fp;
	int error;

	error = getvnode(p->p_fd, SCARG(uap, filedes), &fp);
	if (error)
		return(error);
	return vacl_set_acl(p, (struct vnode *)fp->f_data, SCARG(uap, type),
	    SCARG(uap, aclp));
}

/*
 * Given a file path, delete an ACL from it.
 */
int
__acl_delete_file(struct proc *p, struct __acl_delete_file_args *uap)
{
	struct nameidata nd;
	int error;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	error = namei(&nd);
	if (error)
		return(error);
	error = vacl_delete(p, nd.ni_vp, SCARG(uap, type));
	NDFREE(&nd, 0);
	return (error);
}

/*
 * Given a file path, delete an ACL from it.
 */
int
__acl_delete_fd(struct proc *p, struct __acl_delete_fd_args *uap)
{
	struct file *fp;
	int error;

	error = getvnode(p->p_fd, SCARG(uap, filedes), &fp);
	if (error)
		return(error);
	error = vacl_delete(p, (struct vnode *)fp->f_data, SCARG(uap, type));
	return (error);
}

/*
 * Given a file path, check an ACL for it
 */
int
__acl_aclcheck_file(struct proc *p, struct __acl_aclcheck_file_args *uap)
{
	struct nameidata	nd;
	int	error;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	error = namei(&nd);
	if (error)
		return(error);
	error = vacl_aclcheck(p, nd.ni_vp, SCARG(uap, type), SCARG(uap, aclp));
	NDFREE(&nd, 0);
	return (error);
}

/*
 * Given a file descriptor, check an ACL for it
 */
int
__acl_aclcheck_fd(struct proc *p, struct __acl_aclcheck_fd_args *uap)
{
	struct file *fp;
	int error;

	error = getvnode(p->p_fd, SCARG(uap, filedes), &fp);
	if (error)
		return(error);
	return vacl_aclcheck(p, (struct vnode *)fp->f_data, SCARG(uap, type),
	    SCARG(uap, aclp));
}

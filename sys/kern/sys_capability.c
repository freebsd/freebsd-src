/*-
 * Copyright (c) 2008-2011 Robert N. M. Watson
 * Copyright (c) 2010-2011 Jonathan Anderson
 * All rights reserved.
 *
 * This software was developed at the University of Cambridge Computer
 * Laboratory with support from a grant from Google, Inc.
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
 * FreeBSD kernel capability facility.
 *
 * Currently, this file implements only capability mode; capabilities
 * (rights-refined file descriptors) will follow.
 *
 */

#include "opt_capsicum.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/capability.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysproto.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/ucred.h>

#include <security/audit/audit.h>

#include <vm/uma.h>
#include <vm/vm.h>

#ifdef CAPABILITY_MODE

FEATURE(security_capabilities, "Capsicum Capability Mode");

/*
 * System call to enter capability mode for the process.
 */
int
cap_enter(struct thread *td, struct cap_enter_args *uap)
{
	struct ucred *newcred, *oldcred;
	struct proc *p;

	if (IN_CAPABILITY_MODE(td))
		return (0);

	newcred = crget();
	p = td->td_proc;
	PROC_LOCK(p);
	oldcred = p->p_ucred;
	crcopy(newcred, oldcred);
	newcred->cr_flags |= CRED_FLAG_CAPMODE;
	p->p_ucred = newcred;
	PROC_UNLOCK(p);
	crfree(oldcred);
	return (0);
}

/*
 * System call to query whether the process is in capability mode.
 */
int
cap_getmode(struct thread *td, struct cap_getmode_args *uap)
{
	u_int i;

	i = (IN_CAPABILITY_MODE(td)) ? 1 : 0;
	return (copyout(&i, uap->modep, sizeof(i)));
}

#else /* !CAPABILITY_MODE */

int
cap_enter(struct thread *td, struct cap_enter_args *uap)
{

	return (ENOSYS);
}

int
cap_getmode(struct thread *td, struct cap_getmode_args *uap)
{

	return (ENOSYS);
}

#endif /* CAPABILITY_MODE */

#ifdef CAPABILITIES

/*
 * struct capability describes a capability, and is hung off of its struct
 * file f_data field.  cap_file and cap_rightss are static once hooked up, as
 * neither the object it references nor the rights it encapsulates are
 * permitted to change.
 */
struct capability {
	struct file	*cap_object;	/* Underlying object's file. */
	struct file	*cap_file;	/* Back-pointer to cap's file. */
	cap_rights_t	 cap_rights;	/* Mask of rights on object. */
};

/*
 * Capabilities have a fileops vector, but in practice none should ever be
 * called except for fo_close, as the capability will normally not be
 * returned during a file descriptor lookup in the system call code.
 */
static fo_rdwr_t capability_read;
static fo_rdwr_t capability_write;
static fo_truncate_t capability_truncate;
static fo_ioctl_t capability_ioctl;
static fo_poll_t capability_poll;
static fo_kqfilter_t capability_kqfilter;
static fo_stat_t capability_stat;
static fo_close_t capability_close;

static struct fileops capability_ops = {
	.fo_read = capability_read,
	.fo_write = capability_write,
	.fo_truncate = capability_truncate,
	.fo_ioctl = capability_ioctl,
	.fo_poll = capability_poll,
	.fo_kqfilter = capability_kqfilter,
	.fo_stat = capability_stat,
	.fo_close = capability_close,
	.fo_flags = DFLAG_PASSABLE,
};

static struct fileops capability_ops_unpassable = {
	.fo_read = capability_read,
	.fo_write = capability_write,
	.fo_truncate = capability_truncate,
	.fo_ioctl = capability_ioctl,
	.fo_poll = capability_poll,
	.fo_kqfilter = capability_kqfilter,
	.fo_stat = capability_stat,
	.fo_close = capability_close,
	.fo_flags = 0,
};

static uma_zone_t capability_zone;

static void
capability_init(void *dummy __unused)
{

	capability_zone = uma_zcreate("capability", sizeof(struct capability),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	if (capability_zone == NULL)
		panic("capability_init: capability_zone not initialized");
}
SYSINIT(vfs, SI_SUB_VFS, SI_ORDER_ANY, capability_init, NULL);

/*
 * Test whether a capability grants the requested rights.
 */
static int
cap_check(struct capability *c, cap_rights_t rights)
{

	if ((c->cap_rights | rights) != c->cap_rights)
		return (ENOTCAPABLE);
	return (0);
}

/*
 * Extract rights from a capability for monitoring purposes -- not for use in
 * any other way, as we want to keep all capability permission evaluation in
 * this one file.
 */
cap_rights_t
cap_rights(struct file *fp_cap)
{
	struct capability *c;

	KASSERT(fp_cap->f_type == DTYPE_CAPABILITY,
	    ("cap_rights: !capability"));

	c = fp_cap->f_data;
	return (c->cap_rights);
}

/*
 * System call to create a new capability reference to either an existing
 * file object or an an existing capability.
 */
int
cap_new(struct thread *td, struct cap_new_args *uap)
{
	int error, capfd;
	int fd = uap->fd;
	struct file *fp, *fcapp;
	cap_rights_t rights = uap->rights;

	AUDIT_ARG_FD(fd);
	AUDIT_ARG_RIGHTS(rights);
	error = fget(td, fd, &fp);
	if (error)
		return (error);
	AUDIT_ARG_FILE(td->td_proc, fp);
	error = kern_capwrap(td, fp, rights, &fcapp, &capfd);
	if (error)
		return (error);

	/*
	 * Release our reference to the file (kern_capwrap has held a reference
	 * for the filedesc array).
	 */
	fdrop(fp, td);
	td->td_retval[0] = capfd;
	return (0);
}

/*
 * System call to query the rights mask associated with a capability.
 */
int
cap_getrights(struct thread *td, struct cap_getrights_args *uap)
{
	struct capability *cp;
	struct file *fp;
	int error;

	AUDIT_ARG_FD(uap->fd);
	error = fgetcap(td, uap->fd, &fp);
	if (error)
		return (error);
	cp = fp->f_data;
	error = copyout(&cp->cap_rights, uap->rightsp, sizeof(*uap->rightsp));
	fdrop(fp, td);
	return (error);
}

/*
 * Create a capability to wrap around an existing file.
 */
int
kern_capwrap(struct thread *td, struct file *fp, cap_rights_t rights,
    struct file **fcappp, int *capfdp)
{
	struct capability *cp, *cp_old;
	struct file *fp_object;
	int error;

	if ((rights | CAP_MASK_VALID) != CAP_MASK_VALID)
		return (EINVAL);

	/*
	 * If a new capability is being derived from an existing capability,
	 * then the new capability rights must be a subset of the existing
	 * rights.
	 */
	if (fp->f_type == DTYPE_CAPABILITY) {
		cp_old = fp->f_data;
		if ((cp_old->cap_rights | rights) != cp_old->cap_rights)
			return (ENOTCAPABLE);
	}

	/*
	 * Allocate a new file descriptor to hang the capability off of.
	 */
	error = falloc(td, fcappp, capfdp, fp->f_flag);
	if (error)
		return (error);

	/*
	 * Rather than nesting capabilities, directly reference the object an
	 * existing capability references.  There's nothing else interesting
	 * to preserve for future use, as we've incorporated the previous
	 * rights mask into the new one.  This prevents us from having to
	 * deal with capability chains.
	 */
	if (fp->f_type == DTYPE_CAPABILITY)
		fp_object = ((struct capability *)fp->f_data)->cap_object;
	else
		fp_object = fp;
	fhold(fp_object);
	cp = uma_zalloc(capability_zone, M_WAITOK | M_ZERO);
	cp->cap_rights = rights;
	cp->cap_object = fp_object;
	cp->cap_file = *fcappp;
	if (fp->f_flag & DFLAG_PASSABLE)
		finit(*fcappp, fp->f_flag, DTYPE_CAPABILITY, cp,
		    &capability_ops);
	else
		finit(*fcappp, fp->f_flag, DTYPE_CAPABILITY, cp,
		    &capability_ops_unpassable);

	/*
	 * Release our private reference (the proc filedesc still has one).
	 */
	fdrop(*fcappp, td);
	return (0);
}

/*
 * Given a file descriptor, test it against a capability rights mask and then
 * return the file descriptor on which to actually perform the requested
 * operation.  As long as the reference to fp_cap remains valid, the returned
 * pointer in *fp will remain valid, so no extra reference management is
 * required, and the caller should fdrop() fp_cap as normal when done with
 * both.
 */
int
cap_funwrap(struct file *fp_cap, cap_rights_t rights, struct file **fpp)
{
	struct capability *c;
	int error;

	if (fp_cap->f_type != DTYPE_CAPABILITY) {
		*fpp = fp_cap;
		return (0);
	}
	c = fp_cap->f_data;
	error = cap_check(c, rights);
	if (error)
		return (error);
	*fpp = c->cap_object;
	return (0);
}

/*
 * Slightly different routine for memory mapping file descriptors: unwrap the
 * capability and check CAP_MMAP, but also return a bitmask representing the
 * maximum mapping rights the capability allows on the object.
 */
int
cap_funwrap_mmap(struct file *fp_cap, cap_rights_t rights, u_char *maxprotp,
    struct file **fpp)
{
	struct capability *c;
	u_char maxprot;
	int error;

	if (fp_cap->f_type != DTYPE_CAPABILITY) {
		*fpp = fp_cap;
		*maxprotp = VM_PROT_ALL;
		return (0);
	}
	c = fp_cap->f_data;
	error = cap_check(c, rights | CAP_MMAP);
	if (error)
		return (error);
	*fpp = c->cap_object;
	maxprot = 0;
	if (c->cap_rights & CAP_READ)
		maxprot |= VM_PROT_READ;
	if (c->cap_rights & CAP_WRITE)
		maxprot |= VM_PROT_WRITE;
	if (c->cap_rights & CAP_MAPEXEC)
		maxprot |= VM_PROT_EXECUTE;
	*maxprotp = maxprot;
	return (0);
}

/*
 * When a capability is closed, simply drop the reference on the underlying
 * object and free the capability.  fdrop() will handle the case where the
 * underlying object also needs to close, and the caller will have already
 * performed any object-specific lock or mqueue handling.
 */
static int
capability_close(struct file *fp, struct thread *td)
{
	struct capability *c;
	struct file *fp_object;

	KASSERT(fp->f_type == DTYPE_CAPABILITY,
	    ("capability_close: !capability"));

	c = fp->f_data;
	fp->f_ops = &badfileops;
	fp->f_data = NULL;
	fp_object = c->cap_object;
	uma_zfree(capability_zone, c);
	return (fdrop(fp_object, td));
}

/*
 * In general, file descriptor operations should never make it to the
 * capability, only the underlying file descriptor operation vector, so panic
 * if we do turn up here.
 */
static int
capability_read(struct file *fp, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{

	panic("capability_read");
}

static int
capability_write(struct file *fp, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{

	panic("capability_write");
}

static int
capability_truncate(struct file *fp, off_t length, struct ucred *active_cred,
    struct thread *td)
{

	panic("capability_truncate");
}

static int
capability_ioctl(struct file *fp, u_long com, void *data,
    struct ucred *active_cred, struct thread *td)
{

	panic("capability_ioctl");
}

static int
capability_poll(struct file *fp, int events, struct ucred *active_cred,
    struct thread *td)
{

	panic("capability_poll");
}

static int
capability_kqfilter(struct file *fp, struct knote *kn)
{

	panic("capability_kqfilter");
}

static int
capability_stat(struct file *fp, struct stat *sb, struct ucred *active_cred,
    struct thread *td)
{

	panic("capability_stat");
}

#else /* !CAPABILITIES */

/*
 * Stub Capability functions for when options CAPABILITIES isn't compiled
 * into the kernel.
 */
int
cap_new(struct thread *td, struct cap_new_args *uap)
{

	return (ENOSYS);
}

int
cap_getrights(struct thread *td, struct cap_getrights_args *uap)
{

	return (ENOSYS);
}

int
cap_funwrap(struct file *fp_cap, cap_rights_t rights, struct file **fpp)
{

	KASSERT(fp_cap->f_type != DTYPE_CAPABILITY,
	    ("cap_funwrap: saw capability"));

	*fpp = fp_cap;
	return (0);
}

int
cap_funwrap_mmap(struct file *fp_cap, cap_rights_t rights, u_char *maxprotp,
    struct file **fpp)
{

	KASSERT(fp_cap->f_type != DTYPE_CAPABILITY,
	    ("cap_funwrap_mmap: saw capability"));

	*fpp = fp_cap;
	*maxprotp = VM_PROT_ALL;
	return (0);
}

#endif /* CAPABILITIES */


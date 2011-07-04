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
 * permitted to change.  cap_filelist may change when other capabilites are
 * added or removed from the same file, and is currently protected by the
 * pool mutex for the object file descriptor.
 */
struct capability {
	struct file	*cap_object;	/* Underlying object's file. */
	struct file	*cap_file;	/* Back-pointer to cap's file. */
	cap_rights_t	 cap_rights;	/* Mask of rights on object. */
	LIST_ENTRY(capability)	cap_filelist; /* Object's cap list. */
};

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

#else /* !CAPABILITIES */

/*
 * Stub Capability functions for when options CAPABILITIES isn't compiled
 * into the kernel.
 */
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


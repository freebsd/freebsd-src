/*-
 * Copyright (c) 2008-2010 Robert N. M. Watson
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
 *
 * $FreeBSD$
 */

/*
 * Definitions for FreeBSD capabilities facility.
 */
#ifndef _SYS_CAPABILITY_H_
#define	_SYS_CAPABILITY_H_

#include <sys/cdefs.h>
#include <sys/types.h>

#include <sys/file.h>

/*
 * Possible rights on capabilities.
 *
 * Notes:
 * Some system calls don't require a capability in order to perform an
 * operation on an fd.  These include: close, dup, dup2.
 *
 * sendfile is authorized using CAP_READ on the file and CAP_WRITE on the
 * socket.
 *
 * mmap() and aio*() system calls will need special attention as they may
 * involve reads or writes depending a great deal on context.
 */
#define	CAP_READ		0x0000000000000001ULL	/* read/recv */
#define	CAP_WRITE		0x0000000000000002ULL	/* write/send */
#define	CAP_MMAP		0x0000000000000004ULL	/* mmap */
#define	CAP_MAPEXEC		0x0000000000000008ULL	/* mmap(2) as exec */
#define	CAP_MASK_VALID		0x000000000000000fULL

#ifdef _KERNEL

#define IN_CAPABILITY_MODE(td) (td->td_ucred->cr_flags & CRED_FLAG_CAPMODE)

/*
 * Create a capability to wrap a file object.
 */
int	kern_capwrap(struct thread *td, struct file *fp, cap_rights_t rights,
	    struct file **cap, int *capfd);

/*
 * Unwrap a capability if its rights mask is a superset of 'rights'.
 *
 * Unwrapping a non-capability is effectively a no-op; the value of fp_cap
 * is simply copied into fpp.
 */
int	cap_funwrap(struct file *fp_cap, cap_rights_t rights,
	    struct file **fpp);
int	cap_funwrap_mmap(struct file *fp_cap, cap_rights_t rights,
	    u_char *maxprotp, struct file **fpp);

/*
 * For the purposes of procstat(1) and similar tools, allow kern_descrip.c to
 * extract the rights from a capability.  However, this should not be used by
 * kernel code generally, instead cap_funwrap() should be used in order to
 * keep all access control in one place.
 */
cap_rights_t	cap_rights(struct file *fp_cap);

#else /* !_KERNEL */

__BEGIN_DECLS

/*
 * cap_enter(): Cause the process to enter capability mode, which will
 * prevent it from directly accessing global namespaces.  System calls will
 * be limited to process-local, process-inherited, or file descriptor
 * operations.  If already in capability mode, a no-op.
 *
 * Currently, process-inherited operations are not properly handled -- in
 * particular, we're interested in things like waitpid(2), kill(2), etc,
 * being properly constrained.  One possible solution is to introduce process
 * descriptors.
 */
int	cap_enter(void);

/*
 * cap_getmode(): Are we in capability mode?
 */
int	cap_getmode(u_int* modep);

/*
 * cap_new(): Create a new capability derived from an existing file
 * descriptor with the specified rights.  If the existing file descriptor is
 * a capability, then the new rights must be a subset of the existing rights.
 */
int	cap_new(int fd, cap_rights_t rights);

/*
 * cap_getrights(): Query the rights on a capability.
 */
int	cap_getrights(int fd, cap_rights_t *rightsp);

__END_DECLS

#endif /* !_KERNEL */

#endif /* !_SYS_CAPABILITY_H_ */

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

#include "opt_capabilities.h"

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

#ifdef CAPABILITIES

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

#else /* !CAPABILITIES */

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

#endif /* CAPABILITIES */

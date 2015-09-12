/*-
 * Copyright (c) 2015, Adrian Chadd <adrian@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/refcount.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/syscallsubr.h>
#include <sys/cpuset.h>
#include <sys/sx.h>
#include <sys/queue.h>
#include <sys/libkern.h>
#include <sys/limits.h>
#include <sys/bus.h>
#include <sys/interrupt.h>

#include <vm/uma.h>
#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_param.h>
#include <vm/vm_phys.h>
#include <vm/vm_domain.h>

int
sys_numa_setaffinity(struct thread *td, struct numa_setaffinity_args *uap)
{
	int error;
	struct vm_domain_policy vp;
	struct thread *ttd;
	struct proc *p;
	struct cpuset *set;

	set = NULL;
	p = NULL;

	/*
	 * Copy in just the policy information into the policy
	 * struct.  Userland only supplies vm_domain_policy_entry.
	 */
	error = copyin(uap->policy, &vp.p, sizeof(vp.p));
	if (error)
		goto out;

	/*
	 * Ensure the seq number is zero - otherwise seq.h
	 * may get very confused.
	 */
	vp.seq = 0;

	/*
	 * Validate policy.
	 */
	if (vm_domain_policy_validate(&vp) != 0) {
		error = EINVAL;
		goto out;
	}

	/*
	 * Go find the desired proc/tid for this operation.
	 */
	error = cpuset_which(uap->which, uap->id, &p,
	    &ttd, &set);
	if (error)
		goto out;

	/* Only handle CPU_WHICH_TID and CPU_WHICH_PID */
	/*
	 * XXX if cpuset_which is called with WHICH_CPUSET and NULL cpuset,
	 * it'll return ESRCH.  We should just return EINVAL.
	 */
	switch (uap->which) {
	case CPU_WHICH_TID:
		vm_domain_policy_copy(&ttd->td_vm_dom_policy, &vp);
		break;
	case CPU_WHICH_PID:
		vm_domain_policy_copy(&p->p_vm_dom_policy, &vp);
		break;
	default:
		error = EINVAL;
		break;
	}

	PROC_UNLOCK(p);
out:
	if (set)
		cpuset_rel(set);
	return (error);
}

int
sys_numa_getaffinity(struct thread *td, struct numa_getaffinity_args *uap)
{
	int error;
	struct vm_domain_policy vp;
	struct thread *ttd;
	struct proc *p;
	struct cpuset *set;

	set = NULL;
	p = NULL;

	error = cpuset_which(uap->which, uap->id, &p,
	    &ttd, &set);
	if (error)
		goto out;

	/* Only handle CPU_WHICH_TID and CPU_WHICH_PID */
	/*
	 * XXX if cpuset_which is called with WHICH_CPUSET and NULL cpuset,
	 * it'll return ESRCH.  We should just return EINVAL.
	 */
	switch (uap->which) {
	case CPU_WHICH_TID:
		vm_domain_policy_localcopy(&vp, &ttd->td_vm_dom_policy);
		break;
	case CPU_WHICH_PID:
		vm_domain_policy_localcopy(&vp, &p->p_vm_dom_policy);
		break;
	default:
		error = EINVAL;
		break;
	}
	if (p)
		PROC_UNLOCK(p);
	/*
	 * Copy out only the vm_domain_policy_entry part.
	 */
	if (error == 0)
		error = copyout(&vp.p, uap->policy, sizeof(vp.p));
out:
	if (set)
		cpuset_rel(set);
	return (error);
}

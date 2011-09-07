/*-
 * Copyright (c) 2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Edward Tomasz Napierala under sponsorship
 * from the FreeBSD Foundation.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_kdtrace.h"

#include <sys/param.h>
#include <sys/eventhandler.h>
#include <sys/param.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/loginclass.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/racct.h>
#include <sys/resourcevar.h>
#include <sys/sbuf.h>
#include <sys/sched.h>
#include <sys/sdt.h>
#include <sys/sx.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/umtx.h>

#ifdef RCTL
#include <sys/rctl.h>
#endif

#ifdef RACCT

FEATURE(racct, "Resource Accounting");

static struct mtx racct_lock;
MTX_SYSINIT(racct_lock, &racct_lock, "racct lock", MTX_DEF);

static uma_zone_t racct_zone;

static void racct_sub_racct(struct racct *dest, const struct racct *src);
static void racct_sub_cred_locked(struct ucred *cred, int resource,
		uint64_t amount);
static void racct_add_cred_locked(struct ucred *cred, int resource,
		uint64_t amount);

SDT_PROVIDER_DEFINE(racct);
SDT_PROBE_DEFINE3(racct, kernel, rusage, add, add, "struct proc *", "int",
    "uint64_t");
SDT_PROBE_DEFINE3(racct, kernel, rusage, add_failure, add-failure,
    "struct proc *", "int", "uint64_t");
SDT_PROBE_DEFINE3(racct, kernel, rusage, add_cred, add-cred, "struct ucred *",
    "int", "uint64_t");
SDT_PROBE_DEFINE3(racct, kernel, rusage, add_force, add-force, "struct proc *",
    "int", "uint64_t");
SDT_PROBE_DEFINE3(racct, kernel, rusage, set, set, "struct proc *", "int",
    "uint64_t");
SDT_PROBE_DEFINE3(racct, kernel, rusage, set_failure, set-failure,
    "struct proc *", "int", "uint64_t");
SDT_PROBE_DEFINE3(racct, kernel, rusage, sub, sub, "struct proc *", "int",
    "uint64_t");
SDT_PROBE_DEFINE3(racct, kernel, rusage, sub_cred, sub-cred, "struct ucred *",
    "int", "uint64_t");
SDT_PROBE_DEFINE1(racct, kernel, racct, create, create, "struct racct *");
SDT_PROBE_DEFINE1(racct, kernel, racct, destroy, destroy, "struct racct *");
SDT_PROBE_DEFINE2(racct, kernel, racct, join, join, "struct racct *",
    "struct racct *");
SDT_PROBE_DEFINE2(racct, kernel, racct, join_failure, join-failure,
    "struct racct *", "struct racct *");
SDT_PROBE_DEFINE2(racct, kernel, racct, leave, leave, "struct racct *",
    "struct racct *");

int racct_types[] = {
	[RACCT_CPU] =
		RACCT_IN_MILLIONS,
	[RACCT_DATA] =
		RACCT_RECLAIMABLE | RACCT_INHERITABLE | RACCT_DENIABLE,
	[RACCT_STACK] =
		RACCT_RECLAIMABLE | RACCT_INHERITABLE | RACCT_DENIABLE,
	[RACCT_CORE] =
		RACCT_DENIABLE,
	[RACCT_RSS] =
		RACCT_RECLAIMABLE,
	[RACCT_MEMLOCK] =
		RACCT_RECLAIMABLE | RACCT_DENIABLE,
	[RACCT_NPROC] =
		RACCT_RECLAIMABLE | RACCT_DENIABLE,
	[RACCT_NOFILE] =
		RACCT_RECLAIMABLE | RACCT_INHERITABLE | RACCT_DENIABLE,
	[RACCT_VMEM] =
		RACCT_RECLAIMABLE | RACCT_INHERITABLE | RACCT_DENIABLE,
	[RACCT_NPTS] =
		RACCT_RECLAIMABLE | RACCT_DENIABLE | RACCT_SLOPPY,
	[RACCT_SWAP] =
		RACCT_RECLAIMABLE | RACCT_DENIABLE | RACCT_SLOPPY,
	[RACCT_NTHR] =
		RACCT_RECLAIMABLE | RACCT_DENIABLE,
	[RACCT_MSGQQUEUED] =
		RACCT_RECLAIMABLE | RACCT_DENIABLE | RACCT_SLOPPY,
	[RACCT_MSGQSIZE] =
		RACCT_RECLAIMABLE | RACCT_DENIABLE | RACCT_SLOPPY,
	[RACCT_NMSGQ] =
		RACCT_RECLAIMABLE | RACCT_DENIABLE | RACCT_SLOPPY,
	[RACCT_NSEM] =
		RACCT_RECLAIMABLE | RACCT_DENIABLE | RACCT_SLOPPY,
	[RACCT_NSEMOP] =
		RACCT_RECLAIMABLE | RACCT_INHERITABLE | RACCT_DENIABLE,
	[RACCT_NSHM] =
		RACCT_RECLAIMABLE | RACCT_DENIABLE | RACCT_SLOPPY,
	[RACCT_SHMSIZE] =
		RACCT_RECLAIMABLE | RACCT_DENIABLE | RACCT_SLOPPY,
	[RACCT_WALLCLOCK] =
		RACCT_IN_MILLIONS };

static void
racct_add_racct(struct racct *dest, const struct racct *src)
{
	int i;

	mtx_assert(&racct_lock, MA_OWNED);

	/*
	 * Update resource usage in dest.
	 */
	for (i = 0; i <= RACCT_MAX; i++) {
		KASSERT(dest->r_resources[i] >= 0,
		    ("racct propagation meltdown: dest < 0"));
		KASSERT(src->r_resources[i] >= 0,
		    ("racct propagation meltdown: src < 0"));
		dest->r_resources[i] += src->r_resources[i];
	}
}

static void
racct_sub_racct(struct racct *dest, const struct racct *src)
{
	int i;

	mtx_assert(&racct_lock, MA_OWNED);

	/*
	 * Update resource usage in dest.
	 */
	for (i = 0; i <= RACCT_MAX; i++) {
		if (!RACCT_IS_SLOPPY(i)) {
			KASSERT(dest->r_resources[i] >= 0,
			    ("racct propagation meltdown: dest < 0"));
			KASSERT(src->r_resources[i] >= 0,
			    ("racct propagation meltdown: src < 0"));
			KASSERT(src->r_resources[i] <= dest->r_resources[i],
			    ("racct propagation meltdown: src > dest"));
		}
		if (RACCT_IS_RECLAIMABLE(i)) {
			dest->r_resources[i] -= src->r_resources[i];
			if (dest->r_resources[i] < 0) {
				KASSERT(RACCT_IS_SLOPPY(i),
				    ("racct_sub_racct: usage < 0"));
				dest->r_resources[i] = 0;
			}
		}
	}
}

void
racct_create(struct racct **racctp)
{

	SDT_PROBE(racct, kernel, racct, create, racctp, 0, 0, 0, 0);

	KASSERT(*racctp == NULL, ("racct already allocated"));

	*racctp = uma_zalloc(racct_zone, M_WAITOK | M_ZERO);
}

static void
racct_destroy_locked(struct racct **racctp)
{
	int i;
	struct racct *racct;

	SDT_PROBE(racct, kernel, racct, destroy, racctp, 0, 0, 0, 0);

	mtx_assert(&racct_lock, MA_OWNED);
	KASSERT(racctp != NULL, ("NULL racctp"));
	KASSERT(*racctp != NULL, ("NULL racct"));

	racct = *racctp;

	for (i = 0; i <= RACCT_MAX; i++) {
		if (RACCT_IS_SLOPPY(i))
			continue;
		if (!RACCT_IS_RECLAIMABLE(i))
			continue;
		KASSERT(racct->r_resources[i] == 0,
		    ("destroying non-empty racct: "
		    "%ju allocated for resource %d\n",
		    racct->r_resources[i], i));
	}
	uma_zfree(racct_zone, racct);
	*racctp = NULL;
}

void
racct_destroy(struct racct **racct)
{

	mtx_lock(&racct_lock);
	racct_destroy_locked(racct);
	mtx_unlock(&racct_lock);
}

/*
 * Increase consumption of 'resource' by 'amount' for 'racct'
 * and all its parents.  Differently from other cases, 'amount' here
 * may be less than zero.
 */
static void
racct_alloc_resource(struct racct *racct, int resource,
    uint64_t amount)
{

	mtx_assert(&racct_lock, MA_OWNED);
	KASSERT(racct != NULL, ("NULL racct"));

	racct->r_resources[resource] += amount;
	if (racct->r_resources[resource] < 0) {
		KASSERT(RACCT_IS_SLOPPY(resource),
		    ("racct_alloc_resource: usage < 0"));
		racct->r_resources[resource] = 0;
	}
}

/*
 * Increase allocation of 'resource' by 'amount' for process 'p'.
 * Return 0 if it's below limits, or errno, if it's not.
 */
int
racct_add(struct proc *p, int resource, uint64_t amount)
{
#ifdef RCTL
	int error;
#endif

	if (p->p_flag & P_SYSTEM)
		return (0);

	SDT_PROBE(racct, kernel, rusage, add, p, resource, amount, 0, 0);

	/*
	 * We need proc lock to dereference p->p_ucred.
	 */
	PROC_LOCK_ASSERT(p, MA_OWNED);

	mtx_lock(&racct_lock);
#ifdef RCTL
	error = rctl_enforce(p, resource, amount);
	if (error && RACCT_IS_DENIABLE(resource)) {
		SDT_PROBE(racct, kernel, rusage, add_failure, p, resource,
		    amount, 0, 0);
		mtx_unlock(&racct_lock);
		return (error);
	}
#endif
	racct_alloc_resource(p->p_racct, resource, amount);
	racct_add_cred_locked(p->p_ucred, resource, amount);
	mtx_unlock(&racct_lock);

	return (0);
}

static void
racct_add_cred_locked(struct ucred *cred, int resource, uint64_t amount)
{
	struct prison *pr;

	SDT_PROBE(racct, kernel, rusage, add_cred, cred, resource, amount,
	    0, 0);

	racct_alloc_resource(cred->cr_ruidinfo->ui_racct, resource, amount);
	for (pr = cred->cr_prison; pr != NULL; pr = pr->pr_parent)
		racct_alloc_resource(pr->pr_prison_racct->prr_racct, resource,
		    amount);
	racct_alloc_resource(cred->cr_loginclass->lc_racct, resource, amount);
}

/*
 * Increase allocation of 'resource' by 'amount' for credential 'cred'.
 * Doesn't check for limits and never fails.
 *
 * XXX: Shouldn't this ever return an error?
 */
void
racct_add_cred(struct ucred *cred, int resource, uint64_t amount)
{

	mtx_lock(&racct_lock);
	racct_add_cred_locked(cred, resource, amount);
	mtx_unlock(&racct_lock);
}

/*
 * Increase allocation of 'resource' by 'amount' for process 'p'.
 * Doesn't check for limits and never fails.
 */
void
racct_add_force(struct proc *p, int resource, uint64_t amount)
{

	if (p->p_flag & P_SYSTEM)
		return;

	SDT_PROBE(racct, kernel, rusage, add_force, p, resource, amount, 0, 0);

	/*
	 * We need proc lock to dereference p->p_ucred.
	 */
	PROC_LOCK_ASSERT(p, MA_OWNED);

	mtx_lock(&racct_lock);
	racct_alloc_resource(p->p_racct, resource, amount);
	mtx_unlock(&racct_lock);
	racct_add_cred(p->p_ucred, resource, amount);
}

static int
racct_set_locked(struct proc *p, int resource, uint64_t amount)
{
	int64_t diff;
#ifdef RCTL
	int error;
#endif

	if (p->p_flag & P_SYSTEM)
		return (0);

	SDT_PROBE(racct, kernel, rusage, set, p, resource, amount, 0, 0);

	/*
	 * We need proc lock to dereference p->p_ucred.
	 */
	PROC_LOCK_ASSERT(p, MA_OWNED);

	diff = amount - p->p_racct->r_resources[resource];
#ifdef notyet
	KASSERT(diff >= 0 || RACCT_IS_RECLAIMABLE(resource),
	    ("racct_set: usage of non-reclaimable resource %d dropping",
	     resource));
#endif
#ifdef RCTL
	if (diff > 0) {
		error = rctl_enforce(p, resource, diff);
		if (error && RACCT_IS_DENIABLE(resource)) {
			SDT_PROBE(racct, kernel, rusage, set_failure, p,
			    resource, amount, 0, 0);
			return (error);
		}
	}
#endif
	racct_alloc_resource(p->p_racct, resource, diff);
	if (diff > 0)
		racct_add_cred_locked(p->p_ucred, resource, diff);
	else if (diff < 0)
		racct_sub_cred_locked(p->p_ucred, resource, -diff);

	return (0);
}

/*
 * Set allocation of 'resource' to 'amount' for process 'p'.
 * Return 0 if it's below limits, or errno, if it's not.
 *
 * Note that decreasing the allocation always returns 0,
 * even if it's above the limit.
 */
int
racct_set(struct proc *p, int resource, uint64_t amount)
{
	int error;

	mtx_lock(&racct_lock);
	error = racct_set_locked(p, resource, amount);
	mtx_unlock(&racct_lock);
	return (error);
}

void
racct_set_force(struct proc *p, int resource, uint64_t amount)
{
	int64_t diff;

	if (p->p_flag & P_SYSTEM)
		return;

	SDT_PROBE(racct, kernel, rusage, set, p, resource, amount, 0, 0);

	/*
	 * We need proc lock to dereference p->p_ucred.
	 */
	PROC_LOCK_ASSERT(p, MA_OWNED);

	mtx_lock(&racct_lock);
	diff = amount - p->p_racct->r_resources[resource];
	racct_alloc_resource(p->p_racct, resource, diff);
	if (diff > 0)
		racct_add_cred_locked(p->p_ucred, resource, diff);
	else if (diff < 0)
		racct_sub_cred_locked(p->p_ucred, resource, -diff);
	mtx_unlock(&racct_lock);
}

/*
 * Returns amount of 'resource' the process 'p' can keep allocated.
 * Allocating more than that would be denied, unless the resource
 * is marked undeniable.  Amount of already allocated resource does
 * not matter.
 */
uint64_t
racct_get_limit(struct proc *p, int resource)
{

#ifdef RCTL
	return (rctl_get_limit(p, resource));
#else
	return (UINT64_MAX);
#endif
}

/*
 * Returns amount of 'resource' the process 'p' can keep allocated.
 * Allocating more than that would be denied, unless the resource
 * is marked undeniable.  Amount of already allocated resource does
 * matter.
 */
uint64_t
racct_get_available(struct proc *p, int resource)
{

#ifdef RCTL
	return (rctl_get_available(p, resource));
#else
	return (UINT64_MAX);
#endif
}

/*
 * Decrease allocation of 'resource' by 'amount' for process 'p'.
 */
void
racct_sub(struct proc *p, int resource, uint64_t amount)
{

	if (p->p_flag & P_SYSTEM)
		return;

	SDT_PROBE(racct, kernel, rusage, sub, p, resource, amount, 0, 0);

	/*
	 * We need proc lock to dereference p->p_ucred.
	 */
	PROC_LOCK_ASSERT(p, MA_OWNED);
	KASSERT(RACCT_IS_RECLAIMABLE(resource),
	    ("racct_sub: called for non-reclaimable resource %d", resource));

	mtx_lock(&racct_lock);
	KASSERT(amount <= p->p_racct->r_resources[resource],
	    ("racct_sub: freeing %ju of resource %d, which is more "
	     "than allocated %jd for %s (pid %d)", amount, resource,
	    (intmax_t)p->p_racct->r_resources[resource], p->p_comm, p->p_pid));

	racct_alloc_resource(p->p_racct, resource, -amount);
	racct_sub_cred_locked(p->p_ucred, resource, amount);
	mtx_unlock(&racct_lock);
}

static void
racct_sub_cred_locked(struct ucred *cred, int resource, uint64_t amount)
{
	struct prison *pr;

	SDT_PROBE(racct, kernel, rusage, sub_cred, cred, resource, amount,
	    0, 0);

#ifdef notyet
	KASSERT(RACCT_IS_RECLAIMABLE(resource),
	    ("racct_sub_cred: called for non-reclaimable resource %d",
	     resource));
#endif

	racct_alloc_resource(cred->cr_ruidinfo->ui_racct, resource, -amount);
	for (pr = cred->cr_prison; pr != NULL; pr = pr->pr_parent)
		racct_alloc_resource(pr->pr_prison_racct->prr_racct, resource,
		    -amount);
	racct_alloc_resource(cred->cr_loginclass->lc_racct, resource, -amount);
}

/*
 * Decrease allocation of 'resource' by 'amount' for credential 'cred'.
 */
void
racct_sub_cred(struct ucred *cred, int resource, uint64_t amount)
{

	mtx_lock(&racct_lock);
	racct_sub_cred_locked(cred, resource, amount);
	mtx_unlock(&racct_lock);
}

/*
 * Inherit resource usage information from the parent process.
 */
int
racct_proc_fork(struct proc *parent, struct proc *child)
{
	int i, error = 0;

	/*
	 * Create racct for the child process.
	 */
	racct_create(&child->p_racct);

	/*
	 * No resource accounting for kernel processes.
	 */
	if (child->p_flag & P_SYSTEM)
		return (0);

	PROC_LOCK(parent);
	PROC_LOCK(child);
	mtx_lock(&racct_lock);

	/*
	 * Inherit resource usage.
	 */
	for (i = 0; i <= RACCT_MAX; i++) {
		if (parent->p_racct->r_resources[i] == 0 ||
		    !RACCT_IS_INHERITABLE(i))
			continue;

		error = racct_set_locked(child, i,
		    parent->p_racct->r_resources[i]);
		if (error != 0) {
			/*
			 * XXX: The only purpose of these two lines is
			 * to prevent from tripping checks in racct_destroy().
			 */
			for (i = 0; i <= RACCT_MAX; i++)
				racct_set_locked(child, i, 0);
			goto out;
		}
	}

#ifdef RCTL
	error = rctl_proc_fork(parent, child);
	if (error != 0) {
		/*
		 * XXX: The only purpose of these two lines is to prevent from
		 * tripping checks in racct_destroy().
		 */
		for (i = 0; i <= RACCT_MAX; i++)
			racct_set_locked(child, i, 0);
	}
#endif

out:
	if (error != 0)
		racct_destroy_locked(&child->p_racct);
	mtx_unlock(&racct_lock);
	PROC_UNLOCK(child);
	PROC_UNLOCK(parent);

	return (error);
}

void
racct_proc_exit(struct proc *p)
{
	int i;
	uint64_t runtime;

	PROC_LOCK(p);
	/*
	 * We don't need to calculate rux, proc_reap() has already done this.
	 */
	runtime = cputick2usec(p->p_rux.rux_runtime);
#ifdef notyet
	KASSERT(runtime >= p->p_prev_runtime, ("runtime < p_prev_runtime"));
#else
	if (runtime < p->p_prev_runtime)
		runtime = p->p_prev_runtime;
#endif
	mtx_lock(&racct_lock);
	racct_set_locked(p, RACCT_CPU, runtime);

	for (i = 0; i <= RACCT_MAX; i++) {
		if (p->p_racct->r_resources[i] == 0)
			continue;
	    	if (!RACCT_IS_RECLAIMABLE(i))
			continue;
		racct_set_locked(p, i, 0);
	}

	mtx_unlock(&racct_lock);
	PROC_UNLOCK(p);

#ifdef RCTL
	rctl_racct_release(p->p_racct);
#endif
	racct_destroy(&p->p_racct);
}

/*
 * Called after credentials change, to move resource utilisation
 * between raccts.
 */
void
racct_proc_ucred_changed(struct proc *p, struct ucred *oldcred,
    struct ucred *newcred)
{
	struct uidinfo *olduip, *newuip;
	struct loginclass *oldlc, *newlc;
	struct prison *oldpr, *newpr, *pr;

	PROC_LOCK_ASSERT(p, MA_NOTOWNED);

	newuip = newcred->cr_ruidinfo;
	olduip = oldcred->cr_ruidinfo;
	newlc = newcred->cr_loginclass;
	oldlc = oldcred->cr_loginclass;
	newpr = newcred->cr_prison;
	oldpr = oldcred->cr_prison;

	mtx_lock(&racct_lock);
	if (newuip != olduip) {
		racct_sub_racct(olduip->ui_racct, p->p_racct);
		racct_add_racct(newuip->ui_racct, p->p_racct);
	}
	if (newlc != oldlc) {
		racct_sub_racct(oldlc->lc_racct, p->p_racct);
		racct_add_racct(newlc->lc_racct, p->p_racct);
	}
	if (newpr != oldpr) {
		for (pr = oldpr; pr != NULL; pr = pr->pr_parent)
			racct_sub_racct(pr->pr_prison_racct->prr_racct,
			    p->p_racct);
		for (pr = newpr; pr != NULL; pr = pr->pr_parent)
			racct_add_racct(pr->pr_prison_racct->prr_racct,
			    p->p_racct);
	}
	mtx_unlock(&racct_lock);

#ifdef RCTL
	rctl_proc_ucred_changed(p, newcred);
#endif
}

static void
racctd(void)
{
	struct thread *td;
	struct proc *p;
	struct timeval wallclock;
	uint64_t runtime;

	for (;;) {
		sx_slock(&allproc_lock);

		FOREACH_PROC_IN_SYSTEM(p) {
			if (p->p_state != PRS_NORMAL)
				continue;
			if (p->p_flag & P_SYSTEM)
				continue;

			microuptime(&wallclock);
			timevalsub(&wallclock, &p->p_stats->p_start);
			PROC_LOCK(p);
			PROC_SLOCK(p);
			FOREACH_THREAD_IN_PROC(p, td) {
				ruxagg(p, td);
				thread_lock(td);
				thread_unlock(td);
			}
			runtime = cputick2usec(p->p_rux.rux_runtime);
			PROC_SUNLOCK(p);
#ifdef notyet
			KASSERT(runtime >= p->p_prev_runtime,
			    ("runtime < p_prev_runtime"));
#else
			if (runtime < p->p_prev_runtime)
				runtime = p->p_prev_runtime;
#endif
			p->p_prev_runtime = runtime;
			mtx_lock(&racct_lock);
			racct_set_locked(p, RACCT_CPU, runtime);
			racct_set_locked(p, RACCT_WALLCLOCK,
			    wallclock.tv_sec * 1000000 + wallclock.tv_usec);
			mtx_unlock(&racct_lock);
			PROC_UNLOCK(p);
		}
		sx_sunlock(&allproc_lock);
		pause("-", hz);
	}
}

static struct kproc_desc racctd_kp = {
	"racctd",
	racctd,
	NULL
};
SYSINIT(racctd, SI_SUB_RACCTD, SI_ORDER_FIRST, kproc_start, &racctd_kp);

static void
racct_init(void)
{

	racct_zone = uma_zcreate("racct", sizeof(struct racct),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	/*
	 * XXX: Move this somewhere.
	 */
	prison0.pr_prison_racct = prison_racct_find("0");
}
SYSINIT(racct, SI_SUB_RACCT, SI_ORDER_FIRST, racct_init, NULL);

#else /* !RACCT */

int
racct_add(struct proc *p, int resource, uint64_t amount)
{

	return (0);
}

void
racct_add_cred(struct ucred *cred, int resource, uint64_t amount)
{
}

void
racct_add_force(struct proc *p, int resource, uint64_t amount)
{

	return;
}

int
racct_set(struct proc *p, int resource, uint64_t amount)
{

	return (0);
}

void
racct_set_force(struct proc *p, int resource, uint64_t amount)
{
}

void
racct_sub(struct proc *p, int resource, uint64_t amount)
{
}

void
racct_sub_cred(struct ucred *cred, int resource, uint64_t amount)
{
}

uint64_t
racct_get_limit(struct proc *p, int resource)
{

	return (UINT64_MAX);
}

uint64_t
racct_get_available(struct proc *p, int resource)
{

	return (UINT64_MAX);
}

void
racct_create(struct racct **racctp)
{
}

void
racct_destroy(struct racct **racctp)
{
}

int
racct_proc_fork(struct proc *parent, struct proc *child)
{

	return (0);
}

void
racct_proc_exit(struct proc *p)
{
}

#endif /* !RACCT */

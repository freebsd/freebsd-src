/*-
 * Copyright (c) 1996, 1997, 1998
 *	HD Associates, Inc.  All rights reserved.
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
 *	This product includes software developed by HD Associates, Inc
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY HD ASSOCIATES AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL HD ASSOCIATES OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* p1003_1b: Real Time common code.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_posix.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/syslog.h>
#include <sys/sysproto.h>

#include <posix4/posix4.h>

MALLOC_DEFINE(M_P31B, "p1003.1b", "Posix 1003.1B");

/* The system calls return ENOSYS if an entry is called that is
 * not run-time supported.  I am also logging since some programs
 * start to use this when they shouldn't.  That will be removed if annoying.
 */
int
syscall_not_present(struct thread *td, const char *s, struct nosys_args *uap)
{
	log(LOG_ERR, "cmd %s pid %d tried to use non-present %s\n",
			td->td_proc->p_comm, td->td_proc->p_pid, s);

	/* a " return nosys(p, uap); " here causes a core dump.
	 */

	return ENOSYS;
}

#if !defined(_KPOSIX_PRIORITY_SCHEDULING)

/* Not configured but loadable via a module:
 */

static int sched_attach(void)
{
	return 0;
}

SYSCALL_NOT_PRESENT_GEN(sched_setparam)
SYSCALL_NOT_PRESENT_GEN(sched_getparam)
SYSCALL_NOT_PRESENT_GEN(sched_setscheduler)
SYSCALL_NOT_PRESENT_GEN(sched_getscheduler)
SYSCALL_NOT_PRESENT_GEN(sched_yield)
SYSCALL_NOT_PRESENT_GEN(sched_get_priority_max)
SYSCALL_NOT_PRESENT_GEN(sched_get_priority_min)
SYSCALL_NOT_PRESENT_GEN(sched_rr_get_interval)

#else

/* Configured in kernel version:
 */
static struct ksched *ksched;

static int sched_attach(void)
{
	int ret = ksched_attach(&ksched);

	if (ret == 0)
		p31b_setcfg(CTL_P1003_1B_PRIORITY_SCHEDULING, 1);

	return ret;
}

/* 
 * MPSAFE
 */
int sched_setparam(struct thread *td,
	struct sched_setparam_args *uap)
{
	struct thread *targettd;
	struct proc *targetp;
	int e;
	struct sched_param sched_param;

	e = copyin(uap->param, &sched_param, sizeof(sched_param));
	if (e)
		return (e);

	mtx_lock(&Giant);
	if (uap->pid == 0) {
		targetp = td->td_proc;
		targettd = td;
		PROC_LOCK(targetp);
	} else {
		targetp = pfind(uap->pid);
		if (targetp == NULL) {
			e = ESRCH;
			goto done2;
		}
		targettd = FIRST_THREAD_IN_PROC(targetp); /* XXXKSE */
	}

	e = p_cansched(td, targetp);
	PROC_UNLOCK(targetp);
	if (e == 0) {
		e = ksched_setparam(&td->td_retval[0], ksched, targettd,
			(const struct sched_param *)&sched_param);
	}
done2:
	mtx_unlock(&Giant);
	return (e);
}

/* 
 * MPSAFE
 */
int sched_getparam(struct thread *td,
	struct sched_getparam_args *uap)
{
	int e;
	struct sched_param sched_param;
	struct thread *targettd;
	struct proc *targetp;

	mtx_lock(&Giant);
	if (uap->pid == 0) {
		targetp = td->td_proc;
		targettd = td;
		PROC_LOCK(targetp);
	} else {
		targetp = pfind(uap->pid);
		if (targetp == NULL) {
			e = ESRCH;
			goto done2;
		}
		targettd = FIRST_THREAD_IN_PROC(targetp); /* XXXKSE */
	}

	e = p_cansee(td, targetp);
	PROC_UNLOCK(targetp);
	if (e)
		goto done2;

	e = ksched_getparam(&td->td_retval[0], ksched, targettd, &sched_param);
	if (e == 0)
		e = copyout(&sched_param, uap->param, sizeof(sched_param));
done2:
	mtx_unlock(&Giant);
	return (e);
}

/* 
 * MPSAFE
 */
int sched_setscheduler(struct thread *td,
	struct sched_setscheduler_args *uap)
{
	int e;
	struct sched_param sched_param;
	struct thread *targettd;
	struct proc *targetp;

	e = copyin(uap->param, &sched_param, sizeof(sched_param));
	if (e)
		return (e);

	mtx_lock(&Giant);
	if (uap->pid == 0) {
		targetp = td->td_proc;
		targettd = td;
		PROC_LOCK(targetp);
	} else {
		targetp = pfind(uap->pid);
		if (targetp == NULL) {
			e = ESRCH;
			goto done2;
		}
		targettd = FIRST_THREAD_IN_PROC(targetp); /* XXXKSE */
	}

	e = p_cansched(td, targetp);
	PROC_UNLOCK(targetp);
	if (e == 0) {
		e = ksched_setscheduler(&td->td_retval[0], ksched, targettd,
			uap->policy, (const struct sched_param *)&sched_param);
	}
done2:
	mtx_unlock(&Giant);
	return (e);
}

/* 
 * MPSAFE
 */
int sched_getscheduler(struct thread *td,
	struct sched_getscheduler_args *uap)
{
	int e;
	struct thread *targettd;
	struct proc *targetp;

	mtx_lock(&Giant);
	if (uap->pid == 0) {
		targetp = td->td_proc;
		targettd = td;
		PROC_LOCK(targetp);
	} else {
		targetp = pfind(uap->pid);
		if (targetp == NULL) {
			e = ESRCH;
			goto done2;
		}
		targettd = FIRST_THREAD_IN_PROC(targetp); /* XXXKSE */
	}

	e = p_cansee(td, targetp);
	PROC_UNLOCK(targetp);
	if (e == 0)
		e = ksched_getscheduler(&td->td_retval[0], ksched, targettd);

done2:
	mtx_unlock(&Giant);
	return (e);
}

/* 
 * MPSAFE
 */
int sched_yield(struct thread *td,
	struct sched_yield_args *uap)
{
	int error;

	mtx_lock(&Giant);
	error = ksched_yield(&td->td_retval[0], ksched);
	mtx_unlock(&Giant);
	return (error);
}

/* 
 * MPSAFE
 */
int sched_get_priority_max(struct thread *td,
	struct sched_get_priority_max_args *uap)
{
	int error;

	mtx_lock(&Giant);
	error = ksched_get_priority_max(&td->td_retval[0], ksched, uap->policy);
	mtx_unlock(&Giant);
	return (error);
}

/* 
 * MPSAFE
 */
int sched_get_priority_min(struct thread *td,
	struct sched_get_priority_min_args *uap)
{
	int error;

	mtx_lock(&Giant);
	error = ksched_get_priority_min(&td->td_retval[0], ksched, uap->policy);
	mtx_unlock(&Giant);
	return (error);
}

/* 
 * MPSAFE
 */
int sched_rr_get_interval(struct thread *td,
	struct sched_rr_get_interval_args *uap)
{
	struct timespec timespec;
	int error;

	error = kern_sched_rr_get_interval(td, uap->pid, &timespec);
	if (error == 0)
		error = copyout(&timespec, uap->interval, sizeof(timespec));
	return (error);
}

int kern_sched_rr_get_interval(struct thread *td, pid_t pid,
    struct timespec *ts)
{
	int e;
	struct thread *targettd;
	struct proc *targetp;

	mtx_lock(&Giant);
	if (pid == 0) {
		targettd = td;
		targetp = td->td_proc;
		PROC_LOCK(targetp);
	} else {
		targetp = pfind(pid);
		if (targetp == NULL) {
			mtx_unlock(&Giant);
			return (ESRCH);
		}
		targettd = FIRST_THREAD_IN_PROC(targetp); /* XXXKSE */
	}

	e = p_cansee(td, targetp);
	if (e == 0)
		e = ksched_rr_get_interval(&td->td_retval[0], ksched, targettd,
			ts);
	PROC_UNLOCK(targetp);
	mtx_unlock(&Giant);
	return (e);
}

#endif

static void p31binit(void *notused)
{
	(void) sched_attach();
	p31b_setcfg(CTL_P1003_1B_PAGESIZE, PAGE_SIZE);
}

SYSINIT(p31b, SI_SUB_P1003_1B, SI_ORDER_FIRST, p31binit, NULL);

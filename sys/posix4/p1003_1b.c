/*
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
 *
 * $FreeBSD: src/sys/posix4/p1003_1b.c,v 1.5.2.1 2000/08/03 01:09:59 peter Exp $
 */

/* p1003_1b: Real Time common code.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysent.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/module.h>
#include <sys/sysproto.h>
#include <sys/sysctl.h>

#include <posix4/posix4.h>

MALLOC_DEFINE(M_P31B, "p1003.1b", "Posix 1003.1B");

/* p31b_proc: Return a proc struct corresponding to a pid to operate on.
 *
 * Enforce permission policy.
 *
 * The policy is the same as for sending signals except there
 * is no notion of process groups.
 *
 * pid == 0 means my process.
 *
 * This is disabled until I've got a permission gate in again:
 * only root can do this.
 */

#if 0
/*
 * This is stolen from CANSIGNAL in kern_sig:
 *
 * Can process p, with pcred pc, do "write flavor" operations to process q?
 */
#define CAN_AFFECT(p, pc, q) \
	((pc)->pc_ucred->cr_uid == 0 || \
	    (pc)->p_ruid == (q)->p_cred->p_ruid || \
	    (pc)->pc_ucred->cr_uid == (q)->p_cred->p_ruid || \
	    (pc)->p_ruid == (q)->p_ucred->cr_uid || \
	    (pc)->pc_ucred->cr_uid == (q)->p_ucred->cr_uid)
#else
#define CAN_AFFECT(p, pc, q) ((pc)->pc_ucred->cr_uid == 0)
#endif

/*
 * p31b_proc: Look up a proc from a PID.  If proc is 0 it is
 * my own proc.
 */
int p31b_proc(struct proc *p, pid_t pid, struct proc **pp)
{
	int ret = 0;
	struct proc *other_proc = 0;

	if (pid == 0)
		other_proc = p;
	else
		other_proc = pfind(pid);

	if (other_proc)
	{
		/* Enforce permission policy.
		 */
		if (CAN_AFFECT(p, p->p_cred, other_proc))
			*pp = other_proc;
		else
			ret = EPERM;
	}
	else
		ret = ESRCH;

	return ret;
}

/* The system calls return ENOSYS if an entry is called that is
 * not run-time supported.  I am also logging since some programs
 * start to use this when they shouldn't.  That will be removed if annoying.
 */
int
syscall_not_present(struct proc *p, const char *s, struct nosys_args *uap)
{
	log(LOG_ERR, "cmd %s pid %d tried to use non-present %s\n",
			p->p_comm, p->p_pid, s);

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

int sched_setparam(struct proc *p,
	struct sched_setparam_args *uap)
{
	int e;

	struct sched_param sched_param;
	copyin(uap->param, &sched_param, sizeof(sched_param));

	(void) (0
	|| (e = p31b_proc(p, uap->pid, &p))
	|| (e = ksched_setparam(&p->p_retval[0], ksched, p,
		(const struct sched_param *)&sched_param))
	);

	return e;
}

int sched_getparam(struct proc *p,
	struct sched_getparam_args *uap)
{
	int e;
	struct sched_param sched_param;

	(void) (0
	|| (e = p31b_proc(p, uap->pid, &p))
	|| (e = ksched_getparam(&p->p_retval[0], ksched, p, &sched_param))
	);

	if (!e)
		copyout(&sched_param, uap->param, sizeof(sched_param));

	return e;
}
int sched_setscheduler(struct proc *p,
	struct sched_setscheduler_args *uap)
{
	int e;

	struct sched_param sched_param;
	copyin(uap->param, &sched_param, sizeof(sched_param));

	(void) (0
	|| (e = p31b_proc(p, uap->pid, &p))
	|| (e = ksched_setscheduler(&p->p_retval[0],
	ksched, p, uap->policy,
		(const struct sched_param *)&sched_param))
	);

	return e;
}
int sched_getscheduler(struct proc *p,
	struct sched_getscheduler_args *uap)
{
	int e;
	(void) (0
	|| (e = p31b_proc(p, uap->pid, &p))
	|| (e = ksched_getscheduler(&p->p_retval[0], ksched, p))
	);

	return e;
}
int sched_yield(struct proc *p,
	struct sched_yield_args *uap)
{
	return ksched_yield(&p->p_retval[0], ksched);
}
int sched_get_priority_max(struct proc *p,
	struct sched_get_priority_max_args *uap)
{
	return ksched_get_priority_max(&p->p_retval[0],
	ksched, uap->policy);
}
int sched_get_priority_min(struct proc *p,
	struct sched_get_priority_min_args *uap)
{
	return ksched_get_priority_min(&p->p_retval[0],
	ksched, uap->policy);
}
int sched_rr_get_interval(struct proc *p,
	struct sched_rr_get_interval_args *uap)
{
	int e;

	(void) (0
	|| (e = p31b_proc(p, uap->pid, &p))
	|| (e = ksched_rr_get_interval(&p->p_retval[0], ksched,
	p, uap->interval))
	);

	return e;
}

#endif

static void p31binit(void *notused)
{
	(void) sched_attach();
	p31b_setcfg(CTL_P1003_1B_PAGESIZE, PAGE_SIZE);
}

SYSINIT(p31b, SI_SUB_P1003_1B, SI_ORDER_FIRST, p31binit, NULL);

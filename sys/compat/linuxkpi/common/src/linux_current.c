/*-
 * Copyright (c) 2017 Hans Petter Selasky
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
 */

#include <sys/cdefs.h>
#ifdef __amd64__
#define	DEV_APIC
#elif defined(__i386__)
#include "opt_apic.h"
#endif

#include <linux/compat.h>
#include <linux/completion.h>
#include <linux/mm.h>
#include <linux/kthread.h>
#include <linux/moduleparam.h>

#include <sys/kernel.h>
#include <sys/eventhandler.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <vm/uma.h>

#if defined(__aarch64__) || defined(__arm__) || defined(__amd64__) ||	\
    defined(__i386__)
#include <machine/fpu.h>
#endif

#ifdef DEV_APIC
extern u_int first_msi_irq, num_msi_irqs;
#endif

static eventhandler_tag linuxkpi_thread_dtor_tag;

static uma_zone_t linux_current_zone;
static uma_zone_t linux_mm_zone;

/* check if another thread already has a mm_struct */
static struct mm_struct *
find_other_mm(struct proc *p)
{
	struct thread *td;
	struct task_struct *ts;
	struct mm_struct *mm;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	FOREACH_THREAD_IN_PROC(p, td) {
		ts = td->td_lkpi_task;
		if (ts == NULL)
			continue;
		mm = ts->mm;
		if (mm == NULL)
			continue;
		/* try to share other mm_struct */
		if (atomic_inc_not_zero(&mm->mm_users))
			return (mm);
	}
	return (NULL);
}

int
linux_alloc_current(struct thread *td, int flags)
{
	struct proc *proc;
	struct task_struct *ts;
	struct mm_struct *mm, *mm_other;

	MPASS(td->td_lkpi_task == NULL);

	if ((td->td_pflags & TDP_ITHREAD) != 0 || !THREAD_CAN_SLEEP()) {
		flags &= ~M_WAITOK;
		flags |= M_NOWAIT | M_USE_RESERVE;
	}

	ts = uma_zalloc(linux_current_zone, flags | M_ZERO);
	if (ts == NULL) {
		if ((flags & (M_WAITOK | M_NOWAIT)) == M_WAITOK)
			panic("linux_alloc_current: failed to allocate task");
		return (ENOMEM);
	}
	mm = NULL;

	/* setup new task structure */
	atomic_set(&ts->kthread_flags, 0);
	ts->task_thread = td;
	ts->comm = td->td_name;
	ts->pid = td->td_tid;
	ts->group_leader = ts;
	atomic_set(&ts->usage, 1);
	atomic_set(&ts->state, TASK_RUNNING);
	init_completion(&ts->parked);
	init_completion(&ts->exited);

	proc = td->td_proc;

	PROC_LOCK(proc);
	mm_other = find_other_mm(proc);

	/* use allocated mm_struct as a fallback */
	if (mm_other == NULL) {
		PROC_UNLOCK(proc);
		mm = uma_zalloc(linux_mm_zone, flags | M_ZERO);
		if (mm == NULL) {
			if ((flags & (M_WAITOK | M_NOWAIT)) == M_WAITOK)
				panic(
			    "linux_alloc_current: failed to allocate mm");
			uma_zfree(linux_current_zone, mm);
			return (ENOMEM);
		}

		PROC_LOCK(proc);
		mm_other = find_other_mm(proc);
		if (mm_other == NULL) {
			/* setup new mm_struct */
			init_rwsem(&mm->mmap_sem);
			atomic_set(&mm->mm_count, 1);
			atomic_set(&mm->mm_users, 1);
			/* set mm_struct pointer */
			ts->mm = mm;
			/* clear pointer to not free memory */
			mm = NULL;
		} else {
			ts->mm = mm_other;
		}
	} else {
		ts->mm = mm_other;
	}

	/* store pointer to task struct */
	td->td_lkpi_task = ts;
	PROC_UNLOCK(proc);

	/* free mm_struct pointer, if any */
	uma_zfree(linux_mm_zone, mm);

	return (0);
}

int
linux_set_fpu_ctx(struct task_struct *task)
{
#if defined(__aarch64__) || defined(__arm__) || defined(__amd64__) ||	\
    defined(__i386__)
	if (task->fpu_ctx == NULL && curthread->td_critnest == 0)
		task->fpu_ctx = fpu_kern_alloc_ctx(FPU_KERN_NOWAIT);
#endif
	return (task->fpu_ctx != NULL ? 0 : ENOMEM);
}

struct mm_struct *
linux_get_task_mm(struct task_struct *task)
{
	struct mm_struct *mm;

	mm = task->mm;
	if (mm != NULL) {
		atomic_inc(&mm->mm_users);
		return (mm);
	}
	return (NULL);
}

void
linux_mm_dtor(struct mm_struct *mm)
{
	uma_zfree(linux_mm_zone, mm);
}

void
linux_free_current(struct task_struct *ts)
{
	mmput(ts->mm);
#if defined(__aarch64__) || defined(__arm__) || defined(__amd64__) ||	\
    defined(__i386__)
	if (ts->fpu_ctx != NULL)
		fpu_kern_free_ctx(ts->fpu_ctx);
#endif
	uma_zfree(linux_current_zone, ts);
}

static void
linuxkpi_thread_dtor(void *arg __unused, struct thread *td)
{
	struct task_struct *ts;

	ts = td->td_lkpi_task;
	if (ts == NULL)
		return;

	td->td_lkpi_task = NULL;
	put_task_struct(ts);
}

static struct task_struct *
linux_get_pid_task_int(pid_t pid, const bool do_get)
{
	struct thread *td;
	struct proc *p;
	struct task_struct *ts;

	if (pid > PID_MAX) {
		/* try to find corresponding thread */
		td = tdfind(pid, -1);
		if (td != NULL) {
			ts = td->td_lkpi_task;
			if (do_get && ts != NULL)
				get_task_struct(ts);
			PROC_UNLOCK(td->td_proc);
			return (ts);
		}
	} else {
		/* try to find corresponding procedure */
		p = pfind(pid);
		if (p != NULL) {
			FOREACH_THREAD_IN_PROC(p, td) {
				ts = td->td_lkpi_task;
				if (ts != NULL) {
					if (do_get)
						get_task_struct(ts);
					PROC_UNLOCK(p);
					return (ts);
				}
			}
			PROC_UNLOCK(p);
		}
	}
	return (NULL);
}

struct task_struct *
linux_pid_task(pid_t pid)
{
	return (linux_get_pid_task_int(pid, false));
}

struct task_struct *
linux_get_pid_task(pid_t pid)
{
	return (linux_get_pid_task_int(pid, true));
}

bool
linux_task_exiting(struct task_struct *task)
{
	struct thread *td;
	struct proc *p;
	bool ret;

	ret = false;

	/* try to find corresponding thread */
	td = tdfind(task->pid, -1);
	if (td != NULL) {
		p = td->td_proc;
	} else {
		/* try to find corresponding procedure */
		p = pfind(task->pid);
	}

	if (p != NULL) {
		if ((p->p_flag & P_WEXIT) != 0)
			ret = true;
		PROC_UNLOCK(p);
	}
	return (ret);
}

static int lkpi_task_resrv;
SYSCTL_INT(_compat_linuxkpi, OID_AUTO, task_struct_reserve,
    CTLFLAG_RDTUN | CTLFLAG_NOFETCH, &lkpi_task_resrv, 0,
    "Number of struct task and struct mm to reserve for non-sleepable "
    "allocations");

static void
linux_current_init(void *arg __unused)
{
	TUNABLE_INT_FETCH("compat.linuxkpi.task_struct_reserve",
	    &lkpi_task_resrv);
	if (lkpi_task_resrv == 0) {
#ifdef DEV_APIC
		/*
		 * Number of interrupt threads plus per-cpu callout
		 * SWI threads.
		 */
		lkpi_task_resrv = first_msi_irq + num_msi_irqs + MAXCPU;
#else
		lkpi_task_resrv = 1024;		/* XXXKIB arbitrary */
#endif
	}
	linux_current_zone = uma_zcreate("lkpicurr",
	    sizeof(struct task_struct), NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);
	uma_zone_reserve(linux_current_zone, lkpi_task_resrv);
	uma_prealloc(linux_current_zone, lkpi_task_resrv);
	linux_mm_zone = uma_zcreate("lkpimm",
	    sizeof(struct task_struct), NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);
	uma_zone_reserve(linux_mm_zone, lkpi_task_resrv);
	uma_prealloc(linux_mm_zone, lkpi_task_resrv);

	atomic_thread_fence_seq_cst();

	linuxkpi_thread_dtor_tag = EVENTHANDLER_REGISTER(thread_dtor,
	    linuxkpi_thread_dtor, NULL, EVENTHANDLER_PRI_ANY);
	lkpi_alloc_current = linux_alloc_current;
}
SYSINIT(linux_current, SI_SUB_EVENTHANDLER + 1, SI_ORDER_SECOND,
    linux_current_init, NULL);

static void
linux_current_uninit(void *arg __unused)
{
	struct proc *p;
	struct task_struct *ts;
	struct thread *td;

	lkpi_alloc_current = linux_alloc_current_noop;

	atomic_thread_fence_seq_cst();

	sx_slock(&allproc_lock);
	FOREACH_PROC_IN_SYSTEM(p) {
		PROC_LOCK(p);
		FOREACH_THREAD_IN_PROC(p, td) {
			if ((ts = td->td_lkpi_task) != NULL) {
				td->td_lkpi_task = NULL;
				put_task_struct(ts);
			}
		}
		PROC_UNLOCK(p);
	}
	sx_sunlock(&allproc_lock);

	thread_reap_barrier();

	EVENTHANDLER_DEREGISTER(thread_dtor, linuxkpi_thread_dtor_tag);

	uma_zdestroy(linux_current_zone);
	uma_zdestroy(linux_mm_zone);
}
SYSUNINIT(linux_current, SI_SUB_EVENTHANDLER + 1, SI_ORDER_SECOND,
    linux_current_uninit, NULL);

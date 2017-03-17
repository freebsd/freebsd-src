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
__FBSDID("$FreeBSD$");

#include <linux/compat.h>
#include <linux/mm.h>
#include <linux/kthread.h>

#include <sys/kernel.h>
#include <sys/eventhandler.h>
#include <sys/malloc.h>

static eventhandler_tag linuxkpi_thread_dtor_tag;

static MALLOC_DEFINE(M_LINUX_CURRENT, "linuxcurrent", "LinuxKPI task structure");

int
linux_alloc_current(struct thread *td, int flags)
{
	struct task_struct *ts;
	struct mm_struct *mm;

	MPASS(td->td_lkpi_task == NULL);

	ts = malloc(sizeof(*ts), M_LINUX_CURRENT, flags | M_ZERO);
	if (ts == NULL)
		return (ENOMEM);

	mm = malloc(sizeof(*mm), M_LINUX_CURRENT, flags | M_ZERO);
	if (mm == NULL) {
		free(ts, M_LINUX_CURRENT);
		return (ENOMEM);
	}

	atomic_set(&ts->kthread_flags, 0);
	ts->task_thread = td;
	ts->comm = td->td_name;
	ts->pid = td->td_tid;
	ts->mm = mm;
	ts->state = TASK_RUNNING;

	/* setup mm_struct */
	init_rwsem(&mm->mmap_sem);
	atomic_set(&mm->mm_count, 1);
	atomic_set(&mm->mm_users, 1);
	mm->vmspace = vmspace_acquire_ref(td->td_proc);

	/* store pointer to task struct */
	td->td_lkpi_task = ts;
	return (0);
}

struct mm_struct *
linux_get_task_mm(struct task_struct *task)
{
	struct mm_struct *mm;

	mm = task->mm;
	if (mm != NULL && mm->vmspace != NULL) {
		atomic_inc(&mm->mm_users);
		return (mm);
	}
	return (NULL);
}

void
linux_mm_dtor(struct mm_struct *mm)
{
	if (mm->vmspace != NULL)
		vmspace_free(mm->vmspace);
	free(mm, M_LINUX_CURRENT);
}

void
linux_free_current(struct task_struct *ts)
{
	mmput(ts->mm);
	free(ts, M_LINUX_CURRENT);
}

static void
linuxkpi_thread_dtor(void *arg __unused, struct thread *td)
{
	struct task_struct *ts;

	ts = td->td_lkpi_task;
	if (ts == NULL)
		return;

	td->td_lkpi_task = NULL;
	linux_free_current(ts);
}

static void
linux_current_init(void *arg __unused)
{
	linuxkpi_thread_dtor_tag = EVENTHANDLER_REGISTER(thread_dtor,
	    linuxkpi_thread_dtor, NULL, EVENTHANDLER_PRI_ANY);
}
SYSINIT(linux_current, SI_SUB_EVENTHANDLER, SI_ORDER_SECOND, linux_current_init, NULL);

static void
linux_current_uninit(void *arg __unused)
{
	EVENTHANDLER_DEREGISTER(thread_dtor, linuxkpi_thread_dtor_tag);
}
SYSUNINIT(linux_current, SI_SUB_EVENTHANDLER, SI_ORDER_SECOND, linux_current_uninit, NULL);

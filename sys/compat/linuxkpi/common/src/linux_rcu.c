/*-
 * Copyright (c) 2016 Matt Macy (mmacy@nextbsd.org)
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

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>

#include <ck_epoch.h>

#include <linux/rcupdate.h>
#include <linux/srcu.h>
#include <linux/slab.h>
#include <linux/kernel.h>

struct callback_head {
	ck_epoch_entry_t epoch_entry;
	rcu_callback_t func;
	ck_epoch_record_t *epoch_record;
	struct task task;
};

/*
 * Verify that "struct rcu_head" is big enough to hold "struct
 * callback_head". This has been done to avoid having to add special
 * compile flags for including ck_epoch.h to all clients of the
 * LinuxKPI.
 */
CTASSERT(sizeof(struct rcu_head) >= sizeof(struct callback_head));

static ck_epoch_t linux_epoch;
static	MALLOC_DEFINE(M_LRCU, "lrcu", "Linux RCU");
static	DPCPU_DEFINE(ck_epoch_record_t *, epoch_record);

static void
linux_rcu_runtime_init(void *arg __unused)
{
	ck_epoch_record_t **pcpu_record;
	ck_epoch_record_t *record;
	int i;

	ck_epoch_init(&linux_epoch);

	CPU_FOREACH(i) {
		record = malloc(sizeof(*record), M_LRCU, M_WAITOK | M_ZERO);
		ck_epoch_register(&linux_epoch, record);
		pcpu_record = DPCPU_ID_PTR(i, epoch_record);
		*pcpu_record = record;
	}

	/*
	 * Populate the epoch with 5 * ncpus # of records
	 */
	for (i = 0; i < 5 * mp_ncpus; i++) {
		record = malloc(sizeof(*record), M_LRCU, M_WAITOK | M_ZERO);
		ck_epoch_register(&linux_epoch, record);
		ck_epoch_unregister(record);
	}
}
SYSINIT(linux_rcu_runtime, SI_SUB_LOCK, SI_ORDER_SECOND, linux_rcu_runtime_init, NULL);

static void
linux_rcu_runtime_uninit(void *arg __unused)
{
	ck_epoch_record_t **pcpu_record;
	ck_epoch_record_t *record;
	int i;

	while ((record = ck_epoch_recycle(&linux_epoch)) != NULL)
		free(record, M_LRCU);

	CPU_FOREACH(i) {
		pcpu_record = DPCPU_ID_PTR(i, epoch_record);
		record = *pcpu_record;
		*pcpu_record = NULL;
		free(record, M_LRCU);
	}
}
SYSUNINIT(linux_rcu_runtime, SI_SUB_LOCK, SI_ORDER_SECOND, linux_rcu_runtime_uninit, NULL);

static ck_epoch_record_t *
linux_rcu_get_record(int canblock)
{
	ck_epoch_record_t *record;

	if (__predict_true((record = ck_epoch_recycle(&linux_epoch)) != NULL))
		return (record);
	if ((record = malloc(sizeof(*record), M_LRCU, M_NOWAIT | M_ZERO)) != NULL) {
		ck_epoch_register(&linux_epoch, record);
		return (record);
	} else if (!canblock)
		return (NULL);

	record = malloc(sizeof(*record), M_LRCU, M_WAITOK | M_ZERO);
	ck_epoch_register(&linux_epoch, record);
	return (record);
}

static void
linux_rcu_destroy_object(ck_epoch_entry_t *e)
{
	struct callback_head *rcu;
	uintptr_t offset;

	rcu = container_of(e, struct callback_head, epoch_entry);

	offset = (uintptr_t)rcu->func;

	MPASS(rcu->task.ta_pending == 0);

	if (offset < LINUX_KFREE_RCU_OFFSET_MAX)
		kfree((char *)rcu - offset);
	else
		rcu->func((struct rcu_head *)rcu);
}

static void
linux_rcu_cleaner_func(void *context, int pending __unused)
{
	struct callback_head *rcu = context;
	ck_epoch_record_t *record = rcu->epoch_record;

	ck_epoch_barrier(record);
	ck_epoch_unregister(record);
}

void
linux_rcu_read_lock(void)
{
	ck_epoch_record_t *record;

	sched_pin();
	record = DPCPU_GET(epoch_record);
	MPASS(record != NULL);

	ck_epoch_begin(record, NULL);
}

void
linux_rcu_read_unlock(void)
{
	ck_epoch_record_t *record;

	record = DPCPU_GET(epoch_record);
	ck_epoch_end(record, NULL);
	sched_unpin();
}

void
linux_synchronize_rcu(void)
{
	ck_epoch_record_t *record;

	sched_pin();
	record = DPCPU_GET(epoch_record);
	MPASS(record != NULL);
	ck_epoch_synchronize(record);
	sched_unpin();
}

void
linux_rcu_barrier(void)
{
	ck_epoch_record_t *record;

	record = linux_rcu_get_record(0);
	ck_epoch_barrier(record);
	ck_epoch_unregister(record);
}

void
linux_call_rcu(struct rcu_head *context, rcu_callback_t func)
{
	struct callback_head *ptr = (struct callback_head *)context;
	ck_epoch_record_t *record;

	record = linux_rcu_get_record(0);

	sched_pin();
	MPASS(record != NULL);
	ptr->func = func;
	ptr->epoch_record = record;
	ck_epoch_call(record, &ptr->epoch_entry, linux_rcu_destroy_object);
	TASK_INIT(&ptr->task, 0, linux_rcu_cleaner_func, ptr);
	taskqueue_enqueue(taskqueue_fast, &ptr->task);
	sched_unpin();
}

int
init_srcu_struct(struct srcu_struct *srcu)
{
	ck_epoch_record_t *record;

	record = linux_rcu_get_record(0);
	srcu->ss_epoch_record = record;
	return (0);
}

void
cleanup_srcu_struct(struct srcu_struct *srcu)
{
	ck_epoch_record_t *record;

	record = srcu->ss_epoch_record;
	srcu->ss_epoch_record = NULL;
	ck_epoch_unregister(record);
}

int
srcu_read_lock(struct srcu_struct *srcu)
{
	ck_epoch_begin(srcu->ss_epoch_record, NULL);
	return (0);
}

void
srcu_read_unlock(struct srcu_struct *srcu, int key __unused)
{
	ck_epoch_end(srcu->ss_epoch_record, NULL);
}

void
synchronize_srcu(struct srcu_struct *srcu)
{
	ck_epoch_synchronize(srcu->ss_epoch_record);
}

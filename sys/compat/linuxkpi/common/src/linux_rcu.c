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

struct callback_head;
struct writer_epoch_record {
	ck_epoch_record_t epoch_record;
	struct mtx head_lock;
	struct mtx sync_lock;
	struct task task;
	STAILQ_HEAD(, callback_head) head;
} __aligned(CACHE_LINE_SIZE);

struct callback_head {
	STAILQ_ENTRY(callback_head) entry;
	rcu_callback_t func;
};

struct srcu_epoch_record {
	ck_epoch_record_t epoch_record;
	struct mtx read_lock;
	struct mtx sync_lock;
};

/*
 * Verify that "struct rcu_head" is big enough to hold "struct
 * callback_head". This has been done to avoid having to add special
 * compile flags for including ck_epoch.h to all clients of the
 * LinuxKPI.
 */
CTASSERT(sizeof(struct rcu_head) >= sizeof(struct callback_head));

/*
 * Verify that "epoch_record" is at beginning of "struct
 * writer_epoch_record":
 */
CTASSERT(offsetof(struct writer_epoch_record, epoch_record) == 0);

/*
 * Verify that "epoch_record" is at beginning of "struct
 * srcu_epoch_record":
 */
CTASSERT(offsetof(struct srcu_epoch_record, epoch_record) == 0);

static ck_epoch_t linux_epoch;
static MALLOC_DEFINE(M_LRCU, "lrcu", "Linux RCU");
static DPCPU_DEFINE(ck_epoch_record_t *, linux_reader_epoch_record);
static DPCPU_DEFINE(struct writer_epoch_record *, linux_writer_epoch_record);

static void linux_rcu_cleaner_func(void *, int);

static void
linux_rcu_runtime_init(void *arg __unused)
{
	int i;

	ck_epoch_init(&linux_epoch);

	/* setup reader records */
	CPU_FOREACH(i) {
		ck_epoch_record_t *record;

		record = malloc(sizeof(*record), M_LRCU, M_WAITOK | M_ZERO);
		ck_epoch_register(&linux_epoch, record, NULL);

		DPCPU_ID_SET(i, linux_reader_epoch_record, record);
	}

	/* setup writer records */
	CPU_FOREACH(i) {
		struct writer_epoch_record *record;

		record = malloc(sizeof(*record), M_LRCU, M_WAITOK | M_ZERO);

		ck_epoch_register(&linux_epoch, &record->epoch_record, NULL);
		mtx_init(&record->head_lock, "LRCU-HEAD", NULL, MTX_DEF);
		mtx_init(&record->sync_lock, "LRCU-SYNC", NULL, MTX_DEF);
		TASK_INIT(&record->task, 0, linux_rcu_cleaner_func, record);
		STAILQ_INIT(&record->head);

		DPCPU_ID_SET(i, linux_writer_epoch_record, record);
	}
}
SYSINIT(linux_rcu_runtime, SI_SUB_LOCK, SI_ORDER_SECOND, linux_rcu_runtime_init, NULL);

static void
linux_rcu_runtime_uninit(void *arg __unused)
{
	ck_stack_entry_t *cursor;
	ck_stack_entry_t *next;
	int i;

	/* make sure all callbacks have been called */
	linux_rcu_barrier();

	/* destroy all writer record mutexes */
	CPU_FOREACH(i) {
		struct writer_epoch_record *record;

		record = DPCPU_ID_GET(i, linux_writer_epoch_record);

		mtx_destroy(&record->head_lock);
		mtx_destroy(&record->sync_lock);
	}

	/* free all registered reader and writer records */
	CK_STACK_FOREACH_SAFE(&linux_epoch.records, cursor, next) {
		ck_epoch_record_t *record;

		record = container_of(cursor,
		    struct ck_epoch_record, record_next);
		free(record, M_LRCU);
	}
}
SYSUNINIT(linux_rcu_runtime, SI_SUB_LOCK, SI_ORDER_SECOND, linux_rcu_runtime_uninit, NULL);

static inline struct srcu_epoch_record *
linux_srcu_get_record(void)
{
	struct srcu_epoch_record *record;

	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
	    "linux_srcu_get_record() might sleep");

	/*
	 * NOTE: The only records that are unregistered and can be
	 * recycled are srcu_epoch_records.
	 */
	record = (struct srcu_epoch_record *)ck_epoch_recycle(&linux_epoch, NULL);
	if (__predict_true(record != NULL))
		return (record);

	record = malloc(sizeof(*record), M_LRCU, M_WAITOK | M_ZERO);
	mtx_init(&record->read_lock, "SRCU-READ", NULL, MTX_DEF | MTX_NOWITNESS);
	mtx_init(&record->sync_lock, "SRCU-SYNC", NULL, MTX_DEF | MTX_NOWITNESS);
	ck_epoch_register(&linux_epoch, &record->epoch_record, NULL);

	return (record);
}

static inline void
linux_rcu_synchronize_sub(struct writer_epoch_record *record)
{

	/* protect access to epoch_record */
	mtx_lock(&record->sync_lock);
	ck_epoch_synchronize(&record->epoch_record);
	mtx_unlock(&record->sync_lock);
}

static void
linux_rcu_cleaner_func(void *context, int pending __unused)
{
	struct writer_epoch_record *record;
	struct callback_head *rcu;
	STAILQ_HEAD(, callback_head) head;

	record = context;

	/* move current callbacks into own queue */
	mtx_lock(&record->head_lock);
	STAILQ_INIT(&head);
	STAILQ_CONCAT(&head, &record->head);
	mtx_unlock(&record->head_lock);

	/* synchronize */
	linux_rcu_synchronize_sub(record);

	/* dispatch all callbacks, if any */
	while ((rcu = STAILQ_FIRST(&head)) != NULL) {
		uintptr_t offset;

		STAILQ_REMOVE_HEAD(&head, entry);

		offset = (uintptr_t)rcu->func;

		if (offset < LINUX_KFREE_RCU_OFFSET_MAX)
			kfree((char *)rcu - offset);
		else
			rcu->func((struct rcu_head *)rcu);
	}
}

void
linux_rcu_read_lock(void)
{
	ck_epoch_record_t *record;

	/*
	 * Pin thread to current CPU so that the unlock code gets the
	 * same per-CPU reader epoch record:
	 */
	sched_pin();

	record = DPCPU_GET(linux_reader_epoch_record);

	/*
	 * Use a critical section to prevent recursion inside
	 * ck_epoch_begin(). Else this function supports recursion.
	 */
	critical_enter();
	ck_epoch_begin(record, NULL);
	critical_exit();
}

void
linux_rcu_read_unlock(void)
{
	ck_epoch_record_t *record;

	record = DPCPU_GET(linux_reader_epoch_record);

	/*
	 * Use a critical section to prevent recursion inside
	 * ck_epoch_end(). Else this function supports recursion.
	 */
	critical_enter();
	ck_epoch_end(record, NULL);
	critical_exit();

	sched_unpin();
}

void
linux_synchronize_rcu(void)
{
	linux_rcu_synchronize_sub(DPCPU_GET(linux_writer_epoch_record));
}

void
linux_rcu_barrier(void)
{
	int i;

	CPU_FOREACH(i) {
		struct writer_epoch_record *record;

		record = DPCPU_ID_GET(i, linux_writer_epoch_record);

		linux_rcu_synchronize_sub(record);

		/* wait for callbacks to complete */
		taskqueue_drain(taskqueue_fast, &record->task);
	}
}

void
linux_call_rcu(struct rcu_head *context, rcu_callback_t func)
{
	struct callback_head *rcu = (struct callback_head *)context;
	struct writer_epoch_record *record;

	record = DPCPU_GET(linux_writer_epoch_record);

	mtx_lock(&record->head_lock);
	rcu->func = func;
	STAILQ_INSERT_TAIL(&record->head, rcu, entry);
	taskqueue_enqueue(taskqueue_fast, &record->task);
	mtx_unlock(&record->head_lock);
}

int
init_srcu_struct(struct srcu_struct *srcu)
{
	struct srcu_epoch_record *record;

	record = linux_srcu_get_record();
	srcu->ss_epoch_record = record;
	return (0);
}

void
cleanup_srcu_struct(struct srcu_struct *srcu)
{
	struct srcu_epoch_record *record;

	record = srcu->ss_epoch_record;
	srcu->ss_epoch_record = NULL;

	ck_epoch_unregister(&record->epoch_record);
}

int
srcu_read_lock(struct srcu_struct *srcu)
{
	struct srcu_epoch_record *record;

	record = srcu->ss_epoch_record;

	mtx_lock(&record->read_lock);
	ck_epoch_begin(&record->epoch_record, NULL);
	mtx_unlock(&record->read_lock);

	return (0);
}

void
srcu_read_unlock(struct srcu_struct *srcu, int key __unused)
{
	struct srcu_epoch_record *record;

	record = srcu->ss_epoch_record;

	mtx_lock(&record->read_lock);
	ck_epoch_end(&record->epoch_record, NULL);
	mtx_unlock(&record->read_lock);
}

void
synchronize_srcu(struct srcu_struct *srcu)
{
	struct srcu_epoch_record *record;

	record = srcu->ss_epoch_record;

	mtx_lock(&record->sync_lock);
	ck_epoch_synchronize(&record->epoch_record);
	mtx_unlock(&record->sync_lock);
}

void
srcu_barrier(struct srcu_struct *srcu)
{
	struct srcu_epoch_record *record;

	record = srcu->ss_epoch_record;

	mtx_lock(&record->sync_lock);
	ck_epoch_barrier(&record->epoch_record);
	mtx_unlock(&record->sync_lock);
}

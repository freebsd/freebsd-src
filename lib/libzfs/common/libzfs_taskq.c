/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2010 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Copyright 2011 Nexenta Systems, Inc.  All rights reserved.
 * Copyright 2012 Garrett D'Amore <garrett@damore.org>.  All rights reserved.
 * Copyright (c) 2014, 2018 by Delphix. All rights reserved.
 */

#include <thread.h>
#include <synch.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>

#include "libzfs_taskq.h"

#define	ZFS_TASKQ_ACTIVE	0x00010000
#define	ZFS_TASKQ_NAMELEN	31

typedef struct zfs_taskq_ent {
	struct zfs_taskq_ent	*ztqent_next;
	struct zfs_taskq_ent	*ztqent_prev;
	ztask_func_t		*ztqent_func;
	void			*ztqent_arg;
	uintptr_t		ztqent_flags;
} zfs_taskq_ent_t;

struct zfs_taskq {
	char		ztq_name[ZFS_TASKQ_NAMELEN + 1];
	mutex_t		ztq_lock;
	rwlock_t	ztq_threadlock;
	cond_t		ztq_dispatch_cv;
	cond_t		ztq_wait_cv;
	thread_t	*ztq_threadlist;
	int		ztq_flags;
	int		ztq_active;
	int		ztq_nthreads;
	int		ztq_nalloc;
	int		ztq_minalloc;
	int		ztq_maxalloc;
	cond_t		ztq_maxalloc_cv;
	int		ztq_maxalloc_wait;
	zfs_taskq_ent_t	*ztq_freelist;
	zfs_taskq_ent_t	ztq_task;
};

static zfs_taskq_ent_t *
ztask_alloc(zfs_taskq_t *ztq, int ztqflags)
{
	zfs_taskq_ent_t *t;
	timestruc_t ts;
	int err;

again:	if ((t = ztq->ztq_freelist) != NULL &&
	    ztq->ztq_nalloc >= ztq->ztq_minalloc) {
		ztq->ztq_freelist = t->ztqent_next;
	} else {
		if (ztq->ztq_nalloc >= ztq->ztq_maxalloc) {
			if (!(ztqflags & UMEM_NOFAIL))
				return (NULL);

			/*
			 * We don't want to exceed ztq_maxalloc, but we can't
			 * wait for other tasks to complete (and thus free up
			 * task structures) without risking deadlock with
			 * the caller.  So, we just delay for one second
			 * to throttle the allocation rate. If we have tasks
			 * complete before one second timeout expires then
			 * zfs_taskq_ent_free will signal us and we will
			 * immediately retry the allocation.
			 */
			ztq->ztq_maxalloc_wait++;

			ts.tv_sec = 1;
			ts.tv_nsec = 0;
			err = cond_reltimedwait(&ztq->ztq_maxalloc_cv,
			    &ztq->ztq_lock, &ts);

			ztq->ztq_maxalloc_wait--;
			if (err == 0)
				goto again;		/* signaled */
		}
		mutex_exit(&ztq->ztq_lock);

		t = umem_alloc(sizeof (zfs_taskq_ent_t), ztqflags);

		mutex_enter(&ztq->ztq_lock);
		if (t != NULL)
			ztq->ztq_nalloc++;
	}
	return (t);
}

static void
ztask_free(zfs_taskq_t *ztq, zfs_taskq_ent_t *t)
{
	if (ztq->ztq_nalloc <= ztq->ztq_minalloc) {
		t->ztqent_next = ztq->ztq_freelist;
		ztq->ztq_freelist = t;
	} else {
		ztq->ztq_nalloc--;
		mutex_exit(&ztq->ztq_lock);
		umem_free(t, sizeof (zfs_taskq_ent_t));
		mutex_enter(&ztq->ztq_lock);
	}

	if (ztq->ztq_maxalloc_wait)
		VERIFY0(cond_signal(&ztq->ztq_maxalloc_cv));
}

zfs_taskqid_t
zfs_taskq_dispatch(zfs_taskq_t *ztq, ztask_func_t func, void *arg,
    uint_t ztqflags)
{
	zfs_taskq_ent_t *t;

	mutex_enter(&ztq->ztq_lock);
	ASSERT(ztq->ztq_flags & ZFS_TASKQ_ACTIVE);
	if ((t = ztask_alloc(ztq, ztqflags)) == NULL) {
		mutex_exit(&ztq->ztq_lock);
		return (0);
	}
	if (ztqflags & ZFS_TQ_FRONT) {
		t->ztqent_next = ztq->ztq_task.ztqent_next;
		t->ztqent_prev = &ztq->ztq_task;
	} else {
		t->ztqent_next = &ztq->ztq_task;
		t->ztqent_prev = ztq->ztq_task.ztqent_prev;
	}
	t->ztqent_next->ztqent_prev = t;
	t->ztqent_prev->ztqent_next = t;
	t->ztqent_func = func;
	t->ztqent_arg = arg;
	t->ztqent_flags = 0;
	VERIFY0(cond_signal(&ztq->ztq_dispatch_cv));
	mutex_exit(&ztq->ztq_lock);
	return (1);
}

void
zfs_taskq_wait(zfs_taskq_t *ztq)
{
	mutex_enter(&ztq->ztq_lock);
	while (ztq->ztq_task.ztqent_next != &ztq->ztq_task ||
	    ztq->ztq_active != 0) {
		int ret = cond_wait(&ztq->ztq_wait_cv, &ztq->ztq_lock);
		VERIFY(ret == 0 || ret == EINTR);
	}
	mutex_exit(&ztq->ztq_lock);
}

static void *
zfs_taskq_thread(void *arg)
{
	zfs_taskq_t *ztq = arg;
	zfs_taskq_ent_t *t;
	boolean_t prealloc;

	mutex_enter(&ztq->ztq_lock);
	while (ztq->ztq_flags & ZFS_TASKQ_ACTIVE) {
		if ((t = ztq->ztq_task.ztqent_next) == &ztq->ztq_task) {
			int ret;
			if (--ztq->ztq_active == 0)
				VERIFY0(cond_broadcast(&ztq->ztq_wait_cv));
			ret = cond_wait(&ztq->ztq_dispatch_cv, &ztq->ztq_lock);
			VERIFY(ret == 0 || ret == EINTR);
			ztq->ztq_active++;
			continue;
		}
		t->ztqent_prev->ztqent_next = t->ztqent_next;
		t->ztqent_next->ztqent_prev = t->ztqent_prev;
		t->ztqent_next = NULL;
		t->ztqent_prev = NULL;
		prealloc = t->ztqent_flags & ZFS_TQENT_FLAG_PREALLOC;
		mutex_exit(&ztq->ztq_lock);

		VERIFY0(rw_rdlock(&ztq->ztq_threadlock));
		t->ztqent_func(t->ztqent_arg);
		VERIFY0(rw_unlock(&ztq->ztq_threadlock));

		mutex_enter(&ztq->ztq_lock);
		if (!prealloc)
			ztask_free(ztq, t);
	}
	ztq->ztq_nthreads--;
	VERIFY0(cond_broadcast(&ztq->ztq_wait_cv));
	mutex_exit(&ztq->ztq_lock);
	return (NULL);
}

/*ARGSUSED*/
zfs_taskq_t *
zfs_taskq_create(const char *name, int nthreads, pri_t pri, int minalloc,
    int maxalloc, uint_t flags)
{
	zfs_taskq_t *ztq = umem_zalloc(sizeof (zfs_taskq_t), UMEM_NOFAIL);
	int t;

	ASSERT3S(nthreads, >=, 1);

	VERIFY0(rwlock_init(&ztq->ztq_threadlock, USYNC_THREAD, NULL));
	VERIFY0(cond_init(&ztq->ztq_dispatch_cv, USYNC_THREAD, NULL));
	VERIFY0(cond_init(&ztq->ztq_wait_cv, USYNC_THREAD, NULL));
	VERIFY0(cond_init(&ztq->ztq_maxalloc_cv, USYNC_THREAD, NULL));
	VERIFY0(mutex_init(
	    &ztq->ztq_lock, LOCK_NORMAL | LOCK_ERRORCHECK, NULL));

	(void) strncpy(ztq->ztq_name, name, ZFS_TASKQ_NAMELEN + 1);

	ztq->ztq_flags = flags | ZFS_TASKQ_ACTIVE;
	ztq->ztq_active = nthreads;
	ztq->ztq_nthreads = nthreads;
	ztq->ztq_minalloc = minalloc;
	ztq->ztq_maxalloc = maxalloc;
	ztq->ztq_task.ztqent_next = &ztq->ztq_task;
	ztq->ztq_task.ztqent_prev = &ztq->ztq_task;
	ztq->ztq_threadlist =
	    umem_alloc(nthreads * sizeof (thread_t), UMEM_NOFAIL);

	if (flags & ZFS_TASKQ_PREPOPULATE) {
		mutex_enter(&ztq->ztq_lock);
		while (minalloc-- > 0)
			ztask_free(ztq, ztask_alloc(ztq, UMEM_NOFAIL));
		mutex_exit(&ztq->ztq_lock);
	}

	for (t = 0; t < nthreads; t++) {
		(void) thr_create(0, 0, zfs_taskq_thread,
		    ztq, THR_BOUND, &ztq->ztq_threadlist[t]);
	}

	return (ztq);
}

void
zfs_taskq_destroy(zfs_taskq_t *ztq)
{
	int t;
	int nthreads = ztq->ztq_nthreads;

	zfs_taskq_wait(ztq);

	mutex_enter(&ztq->ztq_lock);

	ztq->ztq_flags &= ~ZFS_TASKQ_ACTIVE;
	VERIFY0(cond_broadcast(&ztq->ztq_dispatch_cv));

	while (ztq->ztq_nthreads != 0) {
		int ret = cond_wait(&ztq->ztq_wait_cv, &ztq->ztq_lock);
		VERIFY(ret == 0 || ret == EINTR);
	}

	ztq->ztq_minalloc = 0;
	while (ztq->ztq_nalloc != 0) {
		ASSERT(ztq->ztq_freelist != NULL);
		ztask_free(ztq, ztask_alloc(ztq, UMEM_NOFAIL));
	}

	mutex_exit(&ztq->ztq_lock);

	for (t = 0; t < nthreads; t++)
		(void) thr_join(ztq->ztq_threadlist[t], NULL, NULL);

	umem_free(ztq->ztq_threadlist, nthreads * sizeof (thread_t));

	VERIFY0(rwlock_destroy(&ztq->ztq_threadlock));
	VERIFY0(cond_destroy(&ztq->ztq_dispatch_cv));
	VERIFY0(cond_destroy(&ztq->ztq_wait_cv));
	VERIFY0(cond_destroy(&ztq->ztq_maxalloc_cv));
	VERIFY0(mutex_destroy(&ztq->ztq_lock));

	umem_free(ztq, sizeof (zfs_taskq_t));
}

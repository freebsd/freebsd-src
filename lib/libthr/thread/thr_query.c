/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2026 The FreeBSD Foundation
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#include <sys/types.h>
#include <sys/queue.h>

#include <string.h>
#include <pthread.h>
#include <pthread_np.h>

#include "libc_private.h"
#include "thr_private.h"

static void
pthread_getinfo(struct pthread *curthread, struct pthread *thread, bool locked,
    struct pthread_info_np *info)
{
	if (!locked)
		THR_THREAD_LOCK(curthread, thread);
	info->pthread_info_size = sizeof(*info);
	info->thread = thread;
	info->lwpid = thread->tid;
	if (thread->state == PS_DEAD)
		info->state = PTHREAD_STATE_DEAD_NP;
	else if ((thread->flags & THR_FLAGS_SUSPENDED) != 0)
		info->state = PTHREAD_STATE_SUSPENDED_NP;
	else
		info->state = PTHREAD_STATE_RUNNING_NP;
	info->flags = 0;
	if ((thread->flags & THR_FLAGS_DETACHED) != 0)
		info->flags |= PTHREAD_INFO_DETACHED_NP;
	if (thread == _thr_initial)
		info->flags |= PTHREAD_INFO_INITIAL_NP;
	info->cancel_state = 0;
	if (thread->cancel_enable)
		info->cancel_state |= PTHREAD_INFO_CANCEL_ENABLED_NP;
	if (thread->cancel_async)
		info->cancel_state |= PTHREAD_INFO_CANCEL_ASYNC_NP;
	if (thread->cancel_point)
		info->cancel_state |= PTHREAD_INFO_CANCEL_AT_POINT_NP;
	if (thread->cancel_pending)
		info->cancel_state |= PTHREAD_INFO_CANCEL_PENDING_NP;
	if (thread->cancelling)
		info->cancel_state |= PTHREAD_INFO_CANCEL_IN_PROG_NP;
	info->errno_location = thread == _thr_initial ? &__libsys_errno :
	    &thread->error;
	info->tcb = thread->tcb;
	if (thread->name != NULL)
		strlcpy(info->name, thread->name, sizeof(info->name));
	else
		info->name[0] = '\0';
	info->exterr = &thread->uexterr;
	THR_THREAD_UNLOCK(curthread, thread);
}

int
pthread_query_np(pthread_t thread, struct pthread_info_np *info, size_t bufsz)
{
	struct pthread *curthread;
	struct pthread_info_np pi;
	int ret;

	if (_thr_initial == NULL)
		return (EAGAIN);

	curthread = _get_curthread();
	if (thread != curthread) {
		ret = _thr_find_thread(curthread, thread, 1);
		if (ret != 0)
			return (ret);
	}
	pthread_getinfo(curthread, thread, thread != curthread, &pi);
	memcpy(info, &pi, MIN(sizeof(pi), bufsz));
	return (0);
}

int
pthread_query_all_np(struct pthread_info_np *info, size_t *bufsz)
{
	struct pthread *curthread, *thr;
	size_t cnt, i;
	int res;

	if (_thr_initial == NULL) {
		/* Exactly one thread exists. */
		*bufsz = sizeof(*info);
		return (EAGAIN);
	}

	curthread = _get_curthread();
	res = 0;
	i = 0;
	cnt = *bufsz / sizeof(*info);
	THREAD_LIST_RDLOCK(curthread);
	TAILQ_FOREACH(thr, &_thread_list, tle) {
		if (i + 1 > cnt)
			res = ENOMEM;
		/*
		 * Continue the loop even after the buffer is filled,
		 * to report required buffer size.
		 */
		if (res == 0)
			pthread_getinfo(curthread, thr, false, &info[i]);
		i++;
	}
	THREAD_LIST_UNLOCK(curthread);

	*bufsz = i * sizeof(*info);
	return (res);
}

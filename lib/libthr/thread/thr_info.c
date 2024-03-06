/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>
 * Copyright (c) 2018 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
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
 */

#include "namespace.h"
#include <sys/errno.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <pthread_np.h>
#include "un-namespace.h"

#include "thr_private.h"

static void
thr_set_name_np(struct pthread *thread, char **tmp_name)
{

	free(thread->name);
	thread->name = *tmp_name;
	*tmp_name = NULL;
}

/* Set the thread name. */
__weak_reference(_pthread_setname_np, pthread_setname_np);
int
_pthread_setname_np(pthread_t thread, const char *name)
{
	struct pthread *curthread;
	char *tmp_name;
	int res;

	if (name != NULL) {
		tmp_name = strdup(name);
		if (tmp_name == NULL)
			return (ENOMEM);
	} else {
		tmp_name = NULL;
	}
	curthread = _get_curthread();
	if (curthread == thread) {
		res = 0;
		THR_THREAD_LOCK(curthread, thread);
		if (thr_set_name(thread->tid, name) == -1)
			res = errno;
		else
			thr_set_name_np(thread, &tmp_name);
		THR_THREAD_UNLOCK(curthread, thread);
	} else {
		res = ESRCH;
		if (_thr_find_thread(curthread, thread, 0) == 0) {
			if (thread->state != PS_DEAD) {
				if (thr_set_name(thread->tid, name) == -1) {
					res = errno;
				} else {
					thr_set_name_np(thread, &tmp_name);
					res = 0;
				}
			}
			THR_THREAD_UNLOCK(curthread, thread);
		}
	}
	free(tmp_name);
	return (res);
}

/* Set the thread name for debug. */
__weak_reference(_pthread_set_name_np, pthread_set_name_np);
void
_pthread_set_name_np(pthread_t thread, const char *name)
{
	(void)_pthread_setname_np(thread, name);
}

static void
thr_get_name_np(struct pthread *thread, char *buf, size_t len)
{

	if (thread->name != NULL)
		strlcpy(buf, thread->name, len);
	else if (len > 0)
		buf[0] = '\0';
}

__weak_reference(_thr_getname_np, pthread_getname_np);
__weak_reference(_thr_getname_np, _pthread_getname_np);
int
_thr_getname_np(pthread_t thread, char *buf, size_t len)
{
	struct pthread *curthread;
	int res;

	res = 0;
	curthread = _get_curthread();
	if (curthread == thread) {
		THR_THREAD_LOCK(curthread, thread);
		thr_get_name_np(thread, buf, len);
		THR_THREAD_UNLOCK(curthread, thread);
	} else {
		if (_thr_find_thread(curthread, thread, 0) == 0) {
			if (thread->state != PS_DEAD)
				thr_get_name_np(thread, buf, len);
			else
				res = ESRCH;
			THR_THREAD_UNLOCK(curthread, thread);
		} else {
			res = ESRCH;
			if (len > 0)
				buf[0] = '\0';
		}
	}
	return (res);
}

__weak_reference(_pthread_get_name_np, pthread_get_name_np);
void
_pthread_get_name_np(pthread_t thread, char *buf, size_t len)
{
	(void)_thr_getname_np(thread, buf, len);
}

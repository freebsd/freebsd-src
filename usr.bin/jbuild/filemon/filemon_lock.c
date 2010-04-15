/*-
 * Copyright (c) 2009, 2010, Juniper Networks, Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY JUNIPER NETWORKS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL JUNIPER NETWORKS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

__FBSDID("$FreeBSD$");

static void
filemon_filemon_lock(struct filemon *filemon)
{
	mtx_lock(&filemon->mtx);

	while (filemon->locker != NULL && filemon->locker != curthread)
		cv_wait(&filemon->cv, &filemon->mtx);

	filemon->locker = curthread;

	mtx_unlock(&filemon->mtx);
}

static void
filemon_filemon_unlock(struct filemon *filemon)
{
	mtx_lock(&filemon->mtx);

	if (filemon->locker == curthread)
		filemon->locker = NULL;

	/* Wake up threads waiting. */
	cv_broadcast(&filemon->cv);

	mtx_unlock(&filemon->mtx);
}

static void
filemon_lock_read(void)
{
	mtx_lock(&access_mtx);

	while (access_owner != NULL || access_requester != NULL)
		cv_wait(&access_cv, &access_mtx);

	n_readers++;

	/* Wake up threads waiting. */
	cv_broadcast(&access_cv);

	mtx_unlock(&access_mtx);
}

static void
filemon_unlock_read(void)
{
	mtx_lock(&access_mtx);

	if (n_readers > 0)
		n_readers--;

	/* Wake up a thread waiting. */
	cv_broadcast(&access_cv);

	mtx_unlock(&access_mtx);
}

static void
filemon_lock_write(void)
{
	mtx_lock(&access_mtx);

	while (access_owner != curthread) {
		if (access_owner == NULL &&
			(access_requester == NULL || access_requester == curthread)) {
			access_owner = curthread;
			access_requester = NULL;
		} else {
			if (access_requester == NULL)
				access_requester = curthread;

			cv_wait(&access_cv, &access_mtx);
		}
	}

	mtx_unlock(&access_mtx);
}

static void
filemon_unlock_write(void)
{
	mtx_lock(&access_mtx);

	/* Sanity check that the current thread actually has the write lock. */
	if (access_owner == curthread)
		access_owner = NULL;

	/* Wake up a thread waiting. */
	cv_broadcast(&access_cv);

	mtx_unlock(&access_mtx);
}

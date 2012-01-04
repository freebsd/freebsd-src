/*-
 * Copyright (c) 2006, Maxime Henrion <mux@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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
 *
 * $FreeBSD$
 */

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "fixups.h"
#include "misc.h"
#include "queue.h"

/*
 * A synchronized queue to implement fixups.  The updater thread adds
 * fixup requests to the queue with fixups_put() when a checksum
 * mismatch error occurred.  It then calls fixups_close() when he's
 * done requesting fixups.  The detailer thread gets the fixups with
 * fixups_get() and then send the requests to the server.
 *
 * The queue is synchronized with a mutex and a condition variable.
 */

struct fixups {
	pthread_mutex_t lock;
	pthread_cond_t cond;
	STAILQ_HEAD(, fixup) fixupq;
	struct fixup *cur;
	size_t size;
	int closed;
};

static void		 fixups_lock(struct fixups *);
static void		 fixups_unlock(struct fixups *);

static struct fixup	*fixup_new(struct coll *, const char *);
static void		 fixup_free(struct fixup *);

static void
fixups_lock(struct fixups *f)
{
	int error;

	error = pthread_mutex_lock(&f->lock);
	assert(!error);
}

static void
fixups_unlock(struct fixups *f)
{
	int error;

	error = pthread_mutex_unlock(&f->lock);
	assert(!error);
}

static struct fixup *
fixup_new(struct coll *coll, const char *name)
{
	struct fixup *fixup;

	fixup = xmalloc(sizeof(struct fixup));
	fixup->f_name = xstrdup(name);
	fixup->f_coll = coll;
	return (fixup);
}

static void
fixup_free(struct fixup *fixup)
{

	free(fixup->f_name);
	free(fixup);
}

/* Create a new fixup queue. */
struct fixups *
fixups_new(void)
{
	struct fixups *f;

	f = xmalloc(sizeof(struct fixups));
	f->size = 0;
	f->closed = 0;
	f->cur = NULL;
	STAILQ_INIT(&f->fixupq);
	pthread_mutex_init(&f->lock, NULL);
	pthread_cond_init(&f->cond, NULL);
	return (f);
}

/* Add a fixup request to the queue. */
void
fixups_put(struct fixups *f, struct coll *coll, const char *name)
{
	struct fixup *fixup;
	int dosignal;

	dosignal = 0;
	fixup = fixup_new(coll, name);
	fixups_lock(f);
	assert(!f->closed);
	STAILQ_INSERT_TAIL(&f->fixupq, fixup, f_link);
	if (f->size++ == 0)
		dosignal = 1;
	fixups_unlock(f);
	if (dosignal)
		pthread_cond_signal(&f->cond);
}

/* Get a fixup request from the queue. */
struct fixup *
fixups_get(struct fixups *f)
{
	struct fixup *fixup, *tofree;

	fixups_lock(f);
	while (f->size == 0 && !f->closed)
		pthread_cond_wait(&f->cond, &f->lock);
	if (f->closed && f->size == 0) {
		fixups_unlock(f);
		return (NULL);
	}
	assert(f->size > 0);
	fixup = STAILQ_FIRST(&f->fixupq);
	tofree = f->cur;
	f->cur = fixup;
	STAILQ_REMOVE_HEAD(&f->fixupq, f_link);
	f->size--;
	fixups_unlock(f);
	if (tofree != NULL)
		fixup_free(tofree);
	return (fixup);
}

/* Close the writing end of the queue. */
void
fixups_close(struct fixups *f)
{
	int dosignal;

	dosignal = 0;
	fixups_lock(f);
	if (f->size == 0 && !f->closed)
		dosignal = 1;
	f->closed = 1;
	fixups_unlock(f);
	if (dosignal)
		pthread_cond_signal(&f->cond);
}

/* Free a fixups queue. */
void
fixups_free(struct fixups *f)
{
	struct fixup *fixup, *fixup2;

	assert(f->closed);
	/*
	 * Free any fixup that has been left on the queue.
	 * This can happen if we have been aborted prematurely.
	 */
	fixup = STAILQ_FIRST(&f->fixupq);
	while (fixup != NULL) {
		fixup2 = STAILQ_NEXT(fixup, f_link);
		fixup_free(fixup);
		fixup = fixup2;
	}
	if (f->cur != NULL)
		fixup_free(f->cur);
	pthread_cond_destroy(&f->cond);
	pthread_mutex_destroy(&f->lock);
	free(f);
}

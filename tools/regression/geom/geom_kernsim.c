/*-
 * Copyright (c) 2002 Poul-Henning Kamp
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Poul-Henning Kamp
 * and NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sched.h>
#include <err.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <geom/geom.h>
#include <geom/geom_int.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <machine/atomic.h>

#include <pthread.h>

#define NTHREAD 30
static struct thread thr[NTHREAD];

static int weredone;
int g_debugflags;
int bootverbose = 1;

void
done(void)
{
	
	rattle();
	weredone = 1;
	exit(0);
}

void
secrethandshake()
{
	int i;

	if (weredone)
		exit(0);
	for (i = 0; i < NTHREAD; i++) {
		if (thr[i].pid == 0)
			continue;
		if (kill(thr[i].pid, 0)) {
			printf("\n\nMissing process for thread %d (\"%s\")\n",
				i, thr[i].name);
			goto scram;
		}
	}
	return;

scram:
	for (i = 0; i < NTHREAD; i++) {
		if (thr[i].pid == 0)
			continue;
		if (thr[i].pid == getpid())
			continue;
		kill(thr[i].pid, 9);
	}
	exit(9);
}


void
biodone(struct bio *bp)
{
	bp->bio_flags |= BIO_DONE;
	if (bp->bio_done != NULL)
		bp->bio_done(bp);
}

int
biowait(struct bio *bp, const char *wchan __unused)
{

	while ((bp->bio_flags & BIO_DONE) == 0)
		usleep(10000);
	if (!(bp->bio_flags & BIO_ERROR))
		return (0);
	if (bp->bio_error)
		return (bp->bio_error);
	return (EIO);
}

void
wakeup(void *chan)
{
	
	if (chan == &g_wait_event)
		pthread_cond_signal(&ptc_event);
	else if (chan == &g_wait_up)
		pthread_cond_signal(&ptc_up);
	else if (chan == &g_wait_down)
		pthread_cond_signal(&ptc_down);
	else
	return;
}

int hz;

int
tsleep(void *chan __unused, int pri __unused, const char *wmesg __unused, int timo)
{

	usleep(timo);
	return (0);
}

void
rattle()
{
	int i;

	pthread_yield();

	for (;;) {
		i = pthread_mutex_trylock(&ptm_up);
		if (i != EBUSY) {
			i = pthread_mutex_trylock(&ptm_down);
			if (i != EBUSY) {
				i = pthread_mutex_trylock(&ptm_event);
				if (i != EBUSY)
					break;
				pthread_mutex_unlock(&ptm_down);
			}
			pthread_mutex_unlock(&ptm_up);
		}
		usleep(100000);
	}
	pthread_mutex_unlock(&ptm_event);
	pthread_mutex_unlock(&ptm_down);
	pthread_mutex_unlock(&ptm_up);
	return;
}


void
new_thread(void *(*func)(void *arg), const char *name)
{
	struct thread *tp;
	static int nextt;
	int error;

	tp = thr + nextt++;
	error = pthread_create(&tp->tid, NULL, func, tp);
	if (error)
		err(1, "pthread_create(%s)", name);
	tp->name = strdup(name);
	printf("New Thread %d %s %p %d\n", tp - thr, name, tp, tp->pid);
}

#define FASCIST 0

void *
g_malloc(int size, int flags)
{
	void *p;

#if FASCIST
	p = malloc(4096);
	printf("Malloc %p \n", p);
#else
	p = malloc(size);
#endif
	if (flags & M_ZERO)
		memset(p, 0, size);
	else
		memset(p, 0xd0, size);
	return (p);
}

void
g_free(void *ptr)
{

	secrethandshake();
#if FASCIST
	printf("Free %p \n", ptr);
	munmap(ptr, 4096);
#else
	free(ptr);
#endif
}

static pthread_mutex_t pt_topology;

void
g_init(void)
{

	pthread_mutex_init(&pt_topology, NULL);
	g_io_init();
	g_event_init();
}

void
g_topology_lock()
{

	pthread_mutex_lock(&pt_topology);
}

void
g_topology_unlock()
{

	pthread_mutex_unlock(&pt_topology);
}

void
g_topology_assert()
{

#if 0
	if (topology_lock == getpid())
		return;
	KASSERT(1 == 0, ("Lacking topology_lock"));
#endif
}

void
mtx_init(struct mtx *mp, const char *bla __unused, const char *yak __unused, int foo __unused)
{

	pthread_mutex_init((pthread_mutex_t *)&mp->mtx_object, NULL);
}

void
mtx_destroy(struct mtx *mp)
{

	pthread_mutex_destroy((pthread_mutex_t *)&mp->mtx_object);
}

void
mtx_lock(struct mtx *mp)
{

	pthread_mutex_lock((pthread_mutex_t *)&mp->mtx_object);
}

void
mtx_unlock(struct mtx *mp)
{

	pthread_mutex_unlock((pthread_mutex_t *)&mp->mtx_object);
}

struct mtx Giant;


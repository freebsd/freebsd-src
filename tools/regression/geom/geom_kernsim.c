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
#include <sys/types.h>
#include <sys/mman.h>
#include <machine/atomic.h>

#define NTHREAD 30
static struct thread thr[NTHREAD];

static int weredone;
int g_debugflags;
int bootverbose = 1;

void
done(void)
{
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

static int sleeping;

void
wakeup(void *chan)
{
	int i;

	secrethandshake();
	for (i = 0; i < NTHREAD; i++)
		if (thr[i].wchan == chan) {
			// printf("wakeup %s\n", thr[i].name);
			atomic_clear_int(&sleeping, 1 << i);
			write(thr[i].pipe[1], "\0", 1);
		}
}

int hz;

int
tsleep(void *chan, int pri __unused, const char *wmesg, int timo)
{
	fd_set r,w,e;
	int i, j, fd, pid;
	struct timeval tv;
	char buf[100];
	struct thread *tp;

	secrethandshake();
	pid = getpid();
	tp = NULL;
	for (i = 0; i < NTHREAD; i++) {
		if (pid != thr[i].pid)
			continue;
		tp = &thr[i];
		break;
	}

	KASSERT(tp != NULL, ("Confused tp=%p has no name\n", tp));
	KASSERT(tp->name != NULL, ("Confused tp=%p has no name\n", tp));
	tp->wchan = chan;
	tp->wmesg = wmesg;
	fd = tp->pipe[0];
	// printf("tsleep %s %p %s\n", tp->name, chan, wmesg);
	for (;;) {
		if (timo > 0) {
			tv.tv_sec = 1;
			tv.tv_usec = 0;
		} else {
			tv.tv_sec = 10;
			tv.tv_usec = 0;
		}
		FD_ZERO(&r);
		FD_ZERO(&w);
		FD_ZERO(&e);
		FD_SET(fd, &r);
		atomic_set_int(&sleeping, 1 << i);
		j = select(fd + 1, &r, &w, &e, &tv);
		secrethandshake();
		if (j)
			break;
		atomic_set_int(&sleeping, 1 << i);
	}
	i = read(fd, buf, sizeof(buf));
	tp->wchan = 0;
	tp->wmesg = 0;
	return(i);
}

void
rattle()
{
	int i, j;

	for (;;) {
		secrethandshake();
		usleep(100000);
		secrethandshake();
		i = sleeping & 7;
		if (i != 7)
			continue;
		usleep(20000);
		j = sleeping & 7;
		if (i != j)
			continue;
		printf("Rattle(%d) \"%s\" \"%s\" \"%s\"\n", i, thr[0].wmesg, thr[1].wmesg, thr[2].wmesg);
		if (!thr[0].wmesg || strcmp(thr[0].wmesg, "up"))
			continue;
		if (!thr[1].wmesg || strcmp(thr[1].wmesg, "down"))
			continue;
		if (!thr[2].wmesg || strcmp(thr[2].wmesg, "events"))
			continue;
		break;
	}
}


void
new_thread(int (*func)(void *arg), char *name)
{
	char *c;
	struct thread *tp;
	static int nextt;

	tp = thr + nextt++;
	c = calloc(1, 65536);
	tp->name = name;
	pipe(tp->pipe);
	tp->pid = rfork_thread(RFPROC|RFMEM, c + 65536 - 16, func, tp);
	if (tp->pid <= 0)
		err(1, "rfork_thread");
	printf("New Thread %d %s %p %d\n", tp - thr, name, tp, tp->pid);
}

#define FASCIST 0
int malloc_lock;

void *
g_malloc(int size, int flags)
{
	void *p;

	secrethandshake();
	while (!atomic_cmpset_int(&malloc_lock, 0, 1))
		sched_yield();
#if FASCIST
	p = malloc(4096);
	printf("Malloc %p \n", p);
#else
	p = malloc(size);
#endif
	malloc_lock = 0;
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
	while (!atomic_cmpset_int(&malloc_lock, 0, 1))
		sched_yield();
#if FASCIST
	printf("Free %p \n", ptr);
	munmap(ptr, 4096);
#else
	free(ptr);
#endif
	malloc_lock = 0;
}


void
g_init(void)
{

	g_io_init();
	g_event_init();
}

static int topology_lock;

void
g_topology_lock()
{
	int pid;

	pid = getpid();
	if (atomic_cmpset_int(&topology_lock, 0, pid))
		return;
	KASSERT(pid != topology_lock,
	    ("Locking topology against myself pid %d\n", pid));
	while (!atomic_cmpset_int(&topology_lock, 0, pid)) {
		printf("Waiting for topology lock mypid = %d held by %d\n",
		    pid, topology_lock);
		sleep(1);
	}
}

void
g_topology_unlock()
{

	KASSERT(topology_lock == getpid(), ("Didn't have topology_lock to release"));
	topology_lock = 0;
}

void
g_topology_assert()
{

	if (topology_lock == getpid())
		return;
	KASSERT(1 == 0, ("Lacking topology_lock"));
}

void
mtx_lock_spin(struct mtx *mp)
{

	while(!atomic_cmpset_int(&mp->mtx_lock, 0, 1)) {
		printf("trying to get lock...\n");
		sched_yield();
	}
}

void
mtx_unlock_spin(struct mtx *mp)
{

	mp->mtx_lock = 0;
}

void
mtx_init(struct mtx *mp, char *bla __unused, int foo __unused)
{
	mp->mtx_lock = 0;
}

void
mtx_destroy(struct mtx *mp)
{
	mp->mtx_lock = 0;
}

void
mtx_lock(struct mtx *mp)
{

	mp->mtx_lock = 0;
}

void
mtx_unlock(struct mtx *mp)
{

	mp->mtx_lock = 0;
}

struct mtx Giant;


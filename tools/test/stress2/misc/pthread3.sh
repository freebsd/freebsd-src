#!/bin/sh

#
# Copyright (c) 2014 EMC Corp.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# _exit(2) test scenario with focus on shared channel tear down.

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > pthread3.c
mycc -o pthread3 -Wall -Wextra -O2 -g -gdwarf-2 pthread3.c -lpthread || exit 1
rm -f pthread3.c

for i in `jot 8`; do
	/tmp/pthread3 &
	pids="$pids $!"
done
s=0
for i in $pids; do
	wait $i
	e=$?
	[ $e -ne 0 ] && s=$e
done

rm -f /tmp/pthread3

exit $s
EOF
/*
 * Threaded producer-consumer test.
 * Loosly based on work by
 * Andrey Zonov (c) 2012
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#ifdef __FreeBSD__
#include <pthread_np.h>
#define	__NP__
#endif
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

#define	LOCK(x)		plock(&x.mtx)
#define	UNLOCK(x)	punlock(&x.mtx)
#define	SIGNAL(x)	psig(&x.wait)
#define	WAIT(x)		pwait(&x.wait, &x.mtx)

long ncreate, nrename, nunlink;
int max;
char *dirname1;
char *dirname2;

struct file {
	char *name;
	STAILQ_ENTRY(file) next;
};

struct files {
	pthread_mutex_t mtx;
	pthread_cond_t wait;
	STAILQ_HEAD(, file) list;
};

static struct files newfiles;
static struct files renamedfiles;

static void
hand(int i __unused) {	/* handler */
	fprintf(stderr, "max = %d, ncreate = %ld, nrename = %ld, nunlink = %ld\n",
			max, ncreate, nrename, nunlink);
}

static void
ahand(int i __unused) {	/* handler */
	fprintf(stderr, "FAIL\n");
	hand(0);
	_exit(0);
}

void
plock(pthread_mutex_t *l)
{
	int rc;

	if ((rc = pthread_mutex_lock(l)) != 0)
		errc(1, rc, "pthread_mutex_lock");
}

void
punlock(pthread_mutex_t *l)
{
	int rc;

	if ((rc = pthread_mutex_unlock(l)) != 0)
		errc(1, rc, "pthread_mutex_unlock");
}

void
psig(pthread_cond_t *c)
{
	int rc;

	if ((rc = pthread_cond_signal(c)) != 0)
		errc(1, rc, "pthread_cond_signal");
}

void
pwait(pthread_cond_t *c, pthread_mutex_t *l)
{
	int rc;

	if ((rc = pthread_cond_wait(c, l)) != 0)
		errc(1, rc, "pthread_cond_wait");
}

void *
loop_create(void *arg __unused)
{
	int i, j;
	struct file *file;

#ifdef __NP__
	pthread_set_name_np(pthread_self(), __func__);
#endif

	for (i = 0; i < max; i++) {
		file = malloc(sizeof(*file));
		asprintf(&file->name, "%s/filename_too-long:%d", dirname1, i);
		LOCK(newfiles);
		STAILQ_INSERT_TAIL(&newfiles.list, file, next);
		ncreate++;
		UNLOCK(newfiles);
		SIGNAL(newfiles);
		if ((i > 0) && (i % 100000 == 0))
			for (j = 0; j < 10 && ncreate != nrename; j++)
				usleep(400);
	}
	return (NULL);
}

void *
loop_rename(void *arg __unused)
{
	char *filename, *newname;
	struct file *file;

#ifdef	__NP__
	pthread_set_name_np(pthread_self(), __func__);
#endif

	while (nrename < max) {
		LOCK(newfiles);
		while (STAILQ_EMPTY(&newfiles.list)) {
			WAIT(newfiles);
		}
		file = STAILQ_FIRST(&newfiles.list);
		STAILQ_REMOVE_HEAD(&newfiles.list, next);
		UNLOCK(newfiles);
		filename = strrchr(file->name, '/');
		asprintf(&newname, "%s/%s", dirname2, filename);
		nrename++;
		free(file->name);
		file->name = newname;
		LOCK(renamedfiles);
		STAILQ_INSERT_TAIL(&renamedfiles.list, file, next);
		UNLOCK(renamedfiles);
		SIGNAL(renamedfiles);
	}
	return (NULL);
}

void *
loop_unlink(void *arg __unused)
{
	struct file *file;

#ifdef	__NP__
	pthread_set_name_np(pthread_self(), __func__);
#endif

	while (nunlink < max) {
		LOCK(renamedfiles);
		while (STAILQ_EMPTY(&renamedfiles.list)) {
			WAIT(renamedfiles);
		}
		file = STAILQ_FIRST(&renamedfiles.list);
		STAILQ_REMOVE_HEAD(&renamedfiles.list, next);
		nunlink++;
		UNLOCK(renamedfiles);
		free(file->name);
		free(file);
		if (arc4random() % 100 == 1)
			if (arc4random() % 100 == 1)
				if (arc4random() % 100 < 10)
					_exit(0);
	}
	return (NULL);
}

void
test(void)
{
	int i;
	int rc;
	pthread_t tid[3];

	asprintf(&dirname1, "%s.1", "f1");
	asprintf(&dirname2, "%s.2", "f2");
//	max = 15000000;
	max = 50000;

	STAILQ_INIT(&newfiles.list);
	STAILQ_INIT(&renamedfiles.list);

	if ((rc = pthread_mutex_init(&newfiles.mtx, NULL)) != 0)
		errc(1, rc, "pthread_mutex_init()");
	if ((rc = pthread_cond_init(&newfiles.wait, NULL)) != 0)
		errc(1, rc, "pthread_cond_init()");
	if ((rc = pthread_mutex_init(&renamedfiles.mtx, NULL)) != 0)
		errc(1, rc, "pthread_mutex_init()");
	if ((rc = pthread_cond_init(&renamedfiles.wait, NULL)) != 0)
		errc(1, rc, "pthread_cond_init()");

	signal(SIGINFO, hand);
	signal(SIGALRM, ahand);
	alarm(300);
	if ((rc = pthread_create(&tid[0], NULL, loop_create, NULL)) != 0)
		errc(1, rc, "pthread_create()");
	if ((rc = pthread_create(&tid[1], NULL, loop_rename, NULL)) != 0)
		errc(1, rc, "pthread_create()");
	if ((rc = pthread_create(&tid[2], NULL, loop_unlink, NULL)) != 0)
		errc(1, rc, "pthread_create()");

	for (i = 0; i < 3; i++) {
		if ((rc = pthread_join(tid[i], NULL)) != 0)
			errc(1, rc, "pthread_join(%d)", i);
	}

	if ((rc = pthread_mutex_destroy(&newfiles.mtx)) != 0)
		errc(1, rc, "pthread_mutex_destroy(newfiles)");
	if ((rc = pthread_cond_destroy(&newfiles.wait)) != 0)
		errc(1, rc, "pthread_cond_destroy(newfiles)");
	if ((rc = pthread_mutex_destroy(&renamedfiles.mtx)) != 0)
		errc(1, rc, "pthread_mutex_destroy(renamedfiles)");
	if ((rc = pthread_cond_destroy(&renamedfiles.wait)) != 0)
		errc(1, rc, "pthread_cond_destroy(renamedfiles)");
	free(dirname1);
	free(dirname2);

	_exit(0);
}

int
main(void)
{
	int i;

	alarm(1200);
	for (i = 0; i < 1000; i++) {
		if (fork() == 0)
			test();
		wait(NULL);
	}

	return (0);
}

#!/bin/sh

#
# Copyright (c) 2012 Peter Holm <pho@FreeBSD.org>
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

# SUJ rename test scenario by Andrey Zonov <zont@FreeBSD.org>
# "panic: flush_pagedep_deps: MKDIR_PARENT" seen:
# http://people.freebsd.org/~pho/stress/log/suj30.txt

# Hang seen: https://people.freebsd.org/~pho/stress/log/log0337.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > suj30.c
mycc -o suj30 -Wall -Wextra -O2 suj30.c -lpthread
rm -f suj30.c

mount | grep "on $mntpoint " | grep -q md$mdstart && umount $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 4g -u $mdstart
newfs -j md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

for i in `jot 10`; do
	/tmp/suj30 $mntpoint/test-$i 100000 &
done
wait

while mount | grep -q $mntpoint; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/suj30
exit 0
EOF
/*
 * Andrey Zonov (c) 2012
 *
 * compile as `cc -o rename rename.c -lpthread'
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define	LOCK(x)		pthread_mutex_lock(&x.mtx)
#define	UNLOCK(x)	pthread_mutex_unlock(&x.mtx)
#define	SIGNAL(x)	pthread_cond_signal(&x.wait)
#define	WAIT(x)		pthread_cond_wait(&x.wait, &x.mtx)

int max;
int exited;
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

void *loop_create(void *arg __unused);
void *loop_rename(void *arg __unused);
void *loop_unlink(void *arg __unused);

int
main(int argc, char **argv)
{
	int i;
	int rc;
	pthread_t tid[3];

	if (argc != 3)
		errx(1, "usage: pthread_count <dirname> <max>");

	asprintf(&dirname1, "%s.1", argv[1]);
	asprintf(&dirname2, "%s.2", argv[1]);
	if (mkdir(dirname1, 0755) == -1)
		err(1, "mkdir(%s)", dirname1);
	if (mkdir(dirname2, 0755) == -1)
		err(1, "mkdir(%s)", dirname2);
	max = atoi(argv[2]);

	STAILQ_INIT(&newfiles.list);
	STAILQ_INIT(&renamedfiles.list);

	rc = pthread_mutex_init(&newfiles.mtx, NULL);
	if (rc != 0)
		errc(1, rc, "pthread_mutex_init()");
	rc = pthread_cond_init(&newfiles.wait, NULL);
	if (rc != 0)
		errc(1, rc, "pthread_cond_init()");
	rc = pthread_mutex_init(&renamedfiles.mtx, NULL);
	if (rc != 0)
		errc(1, rc, "pthread_mutex_init()");
	rc = pthread_cond_init(&renamedfiles.wait, NULL);
	if (rc != 0)
		errc(1, rc, "pthread_cond_init()");

	rc = pthread_create(&tid[0], NULL, loop_create, NULL);
	if (rc != 0)
		errc(1, rc, "pthread_create()");
	rc = pthread_create(&tid[1], NULL, loop_rename, NULL);
	if (rc != 0)
		errc(1, rc, "pthread_create()");
	rc = pthread_create(&tid[2], NULL, loop_unlink, NULL);
	if (rc != 0)
		errc(1, rc, "pthread_create()");

	for (i = 0; i < 3; i++) {
		rc = pthread_join(tid[i], NULL);
		if (rc != 0)
			errc(1, rc, "pthread_join(%d)", i);
	}

	rc = pthread_mutex_destroy(&newfiles.mtx);
	if (rc != 0)
		errc(1, rc, "pthread_mutex_destroy(newfiles)");
	rc = pthread_cond_destroy(&newfiles.wait);
	if (rc != 0)
		errc(1, rc, "pthread_cond_destroy(newfiles)");
	rc = pthread_mutex_destroy(&renamedfiles.mtx);
	if (rc != 0)
		errc(1, rc, "pthread_mutex_destroy(renamedfiles)");
	rc = pthread_cond_destroy(&renamedfiles.wait);
	if (rc != 0)
		errc(1, rc, "pthread_cond_destroy(renamedfiles)");
	rmdir(dirname1);
	rmdir(dirname2);
	free(dirname1);
	free(dirname2);

	exit(0);
}

void *
loop_create(void *arg __unused)
{
	int i;
	struct file *file;

#ifdef __NP__
	pthread_set_name_np(pthread_self(), __func__);
#endif

	for (i = 0; i < max; i++) {
		file = malloc(sizeof(*file));
		asprintf(&file->name, "%s/filename_too-long:%d", dirname1, i);
		if (mkdir(file->name, 0666) == -1) {
			warn("mkdir(%s)", file->name);
			free(file->name);
			free(file);
			break;
		}
		LOCK(newfiles);
		STAILQ_INSERT_TAIL(&newfiles.list, file, next);
		UNLOCK(newfiles);
		SIGNAL(newfiles);
	}
	exited = 1;
	SIGNAL(newfiles);
	pthread_exit(NULL);
}

void *
loop_rename(void *arg __unused)
{
	char *filename, *newname;
	struct file *file;

#ifdef	__NP__
	pthread_set_name_np(pthread_self(), __func__);
#endif

	for ( ;; ) {
		LOCK(newfiles);
		while (STAILQ_EMPTY(&newfiles.list) && exited < 1)
			WAIT(newfiles);
		if (STAILQ_EMPTY(&newfiles.list) && exited == 1) {
			UNLOCK(newfiles);
			break;
		}
		file = STAILQ_FIRST(&newfiles.list);
		STAILQ_REMOVE_HEAD(&newfiles.list, next);
		UNLOCK(newfiles);
		filename = strrchr(file->name, '/');
		asprintf(&newname, "%s/%s", dirname2, filename);
		if (rename(file->name, newname) == -1)
			err(1, "rename(%s, %s)", file->name, newname);
		free(file->name);
		file->name = newname;
		LOCK(renamedfiles);
		STAILQ_INSERT_TAIL(&renamedfiles.list, file, next);
		UNLOCK(renamedfiles);
		SIGNAL(renamedfiles);
	}
	exited = 2;
	SIGNAL(renamedfiles);
	pthread_exit(NULL);
}

void *
loop_unlink(void *arg __unused)
{
	struct file *file;

#ifdef	__NP__
	pthread_set_name_np(pthread_self(), __func__);
#endif

	for ( ;; ) {
		LOCK(renamedfiles);
		while (STAILQ_EMPTY(&renamedfiles.list) && exited < 2)
			WAIT(renamedfiles);
		if (STAILQ_EMPTY(&renamedfiles.list) && exited == 2) {
			UNLOCK(renamedfiles);
			break;
		}
		file = STAILQ_FIRST(&renamedfiles.list);
		STAILQ_REMOVE_HEAD(&renamedfiles.list, next);
		UNLOCK(renamedfiles);
		rmdir(file->name);
		free(file->name);
		free(file);
	}
	pthread_exit(NULL);
}

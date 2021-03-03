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

# Stress shchan allocations.

. ../default.cfg
[ `swapinfo | wc -l` -eq 1 ] && exit 0 # kstack allocation failed

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > pthread5.c
mycc -o pthread5 -Wall -Wextra -O2 pthread5.c -lpthread || exit 1
rm -f pthread5.c

/tmp/pthread5

rm -f /tmp/pthread5
exit 0
EOF
#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#define ITER 2
#define PARALLEL 1000
#define THREADS 100

pthread_cond_t cond  = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void *
nicethreads(void *data __unused)
{
	struct timespec ts;
	struct timeval tp;

	pthread_mutex_lock(&mutex);
	gettimeofday(&tp, NULL);

	ts.tv_sec  = tp.tv_sec;
	ts.tv_nsec = tp.tv_usec * 1000;
	ts.tv_sec += 30;

	pthread_cond_timedwait(&cond, &mutex, &ts);
	pthread_mutex_unlock(&mutex);

	return (NULL);
}

int
test(void)
{
	int num_thread = THREADS;
	pthread_t tid[num_thread];
	int i, iter, rc;

	for (iter = 0; iter < ITER; iter++) {
		for (i = 0; i < num_thread; i++) {
			if ((rc = pthread_create(&tid[i], NULL, nicethreads,
			    NULL)) != 0)
				errc(1, rc, "pthread_create");
		}
		usleep(20000);
		for (i = 0; i < num_thread; i++) {
			rc = pthread_mutex_lock(&mutex);
			rc = pthread_cond_signal(&cond);
			rc = pthread_mutex_unlock(&mutex);
		}
		for (i = 0; i < num_thread; i++)
			if ((rc = pthread_join(tid[i], NULL)) != 0)
				errc(1, rc, "pthread_join");
	}

	_exit(0);
}

int
main(void)
{
	int i;

	for (i = 0; i < PARALLEL; i++)
		if (fork() == 0)
			test();
	for (i = 0; i < PARALLEL; i++)
		wait(NULL);

	return (0);
}

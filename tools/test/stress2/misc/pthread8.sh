#!/bin/sh

#
# Copyright (c) 2015 EMC Corp.
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

# PTHREAD_PRIO_INHERIT test scenario

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > pthread8.c
mycc -o pthread8 -Wall -Wextra -O0 -g pthread8.c -lpthread || exit 1
rm -f pthread8.c /tmp/pthread8.core

/tmp/pthread8

rm -f /tmp/pthread8
exit 0
EOF
/* $Id: pi.c,v 1.2 2015/01/31 11:36:07 kostik Exp kostik $ */

#include <sys/types.h>
#include <sys/sysctl.h>
#include <err.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

struct runner_arg {
	pthread_mutex_t *locks;
	u_int lock_cnt;
};

int stop;

void *
runner(void *arg)
{
	struct runner_arg *ra;
	pthread_mutex_t *l;
	u_int i;
	int error;

	ra = arg;
	while (stop == 0) {
		for (i = 0; i < ra->lock_cnt; i++) {
			l = &ra->locks[i];
			error = pthread_mutex_lock(l);
			if (error != 0)
				errc(1, error, "pthread_mutex_lock");
			pthread_yield();
		}
		for (i = 0; i < ra->lock_cnt; i++) {
			l = &ra->locks[i];
			error = pthread_mutex_unlock(l);
			if (error != 0)
				errc(1, error, "pthread_mutex_lock");
			pthread_yield();
		}
	}
	return (NULL);
}

int
main(void)
{
	struct runner_arg ra;
	time_t start;
	pthread_t *threads;
	pthread_mutexattr_t mattr;
	u_int i, ncpus;
	int error;
	size_t ncpus_len;

	ncpus_len = sizeof(ncpus);
	error = sysctlbyname("hw.ncpu", &ncpus, &ncpus_len, NULL, 0);
	if (error != 0)
		err(1, "sysctl hw.ncpus");
	threads = calloc(ncpus, sizeof(pthread_t));
	if (threads == NULL)
		err(1, "calloc threads");

	ra.lock_cnt = 100;
	ra.locks = calloc(ra.lock_cnt, sizeof(pthread_mutex_t));
	if (ra.locks == NULL)
		err(1, "calloc locks");
	error = pthread_mutexattr_init(&mattr);
	if (error != 0)
		errc(1, error, "pthread_mutexattr_init");
	error = pthread_mutexattr_setprotocol(&mattr, PTHREAD_PRIO_INHERIT);
	if (error != 0)
		errc(1, error, "pthread_mutexattr_setprotocol PRIO_INHERIT");
	for (i = 0; i < ra.lock_cnt; i++) {
		error = pthread_mutex_init(&ra.locks[i], &mattr);
		if (error != 0)
			errc(1, error, "pthread_mutex_init");
	}

	for (i = 0; i < ncpus; i++) {
		error = pthread_create(&threads[i], NULL, runner, &ra);
		if (error != 0)
			errc(1, error, "pthread_create");
	}
	start = time(NULL);
	while (time(NULL) - start < 180)
		sleep(1);
	stop = 1;
	for (i = 0; i < ncpus; i++)
		pthread_join(threads[i], NULL);

	return (0);
}

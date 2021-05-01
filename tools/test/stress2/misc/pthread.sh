#!/bin/sh

#
# Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
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

# panic: spin lock held too long

# Test program and scenario by Peter Wemm <peter@FreeBSD.org>

. ../default.cfg

odir=`pwd`

cd /tmp
sed '1,/^EOF/d' < $odir/$0 > pth.c
mycc -o pth -Wall pth.c -pthread
rm -f pth.c
cd $odir

for i in `jot 2000`; do
	/tmp/pth 2>/dev/null
done

rm -f /tmp/pth
exit
EOF
#include <pthread.h>

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

static pthread_t worker1_thr;
static pthread_t worker2_thr;

static pthread_mutex_t worker_mtx;
static pthread_cond_t worker_go;
static pthread_cond_t worker_done;

struct workitem {
	struct workitem *next;
	int a, b;
};

struct workitem *head;

static void *
worker(void *arg)
{
	struct workitem *w;

	pthread_detach(pthread_self());
	fprintf(stderr, "WORKER: started %p\n", arg); fflush(stderr);

	for (;;) {
		pthread_mutex_lock(&worker_mtx);
		while (head == NULL) {
			pthread_cond_wait(&worker_go, &worker_mtx);
		}
		w = head;
		head = w->next;
		pthread_mutex_unlock(&worker_mtx);

		fprintf(stderr, "WORKER(%p): got work a=%d b=%d\n", arg, w->a, w->b); fflush(stderr);
		free(w);
		pthread_cond_signal(&worker_done);
	}
}

void
work_add(int a, int b)
{
	struct workitem *w;
	int dowake = 0;

	w = calloc(sizeof(*w), 1);
	w->a = a;
	w->b = b;
	pthread_mutex_lock(&worker_mtx);
	if (head == 0)
		dowake = 1;
	w->next = head;
	head = w;
	pthread_mutex_unlock(&worker_mtx);
	if (dowake)
		pthread_cond_signal(&worker_go);
}

int
main()
{
        pthread_mutex_init(&worker_mtx, NULL);
        pthread_cond_init(&worker_go, NULL);
        pthread_cond_init(&worker_done, NULL);

	fprintf(stderr, "pthread create\n"); fflush(stderr);
        pthread_create(&worker1_thr, NULL, worker, (void *)1);
        pthread_create(&worker2_thr, NULL, worker, (void *)2);

	work_add(10, 15);
	work_add(11, 22);
	work_add(314, 159);

	pthread_mutex_lock(&worker_mtx);
	while (head != NULL) {
		pthread_cond_wait(&worker_done, &worker_mtx);
	}
	pthread_mutex_unlock(&worker_mtx);

	fprintf(stderr, "job complete\n"); fflush(stderr);
	exit(0);
}

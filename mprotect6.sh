#!/bin/sh

#
# Copyright (c) 2025 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

. ../default.cfg
set -u
prog=$(basename "$0" .sh)
cat > /tmp/$prog.c <<EOF
#include <sys/types.h>
#include <sys/mman.h>

#include <assert.h>
#include <err.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static pthread_mutex_t write_mutex;
static volatile int done;
static int go, n, *once, *p, ps;

static void *
wr(void *arg)
{
	int idx;

	alarm(180);
	idx = *(int *)arg;
	while (go == 0)
		usleep(100);
	while (go == 1) {
		while (go == 1 && once[idx] == 0)
			usleep(100);
		if (go == 0)
			break;
		p[idx]++;
		once[idx] = 0;
		pthread_mutex_lock(&write_mutex);
		done++;
		pthread_mutex_unlock(&write_mutex);
	}
	return(NULL);
}

static void
setonce(int val)
{
	int i;

	for (i = 0; i < n; i++)
		once[i] = val;
}

static void
usage(char *prog) {
	fprintf(stderr, "Usage: %s <number of threads>\n", prog);
	_exit(1);
}

int
main(int argc, char *argv[])
{
	pthread_t *tid;
	time_t start;
	int *arg;
	int e, i, nb, r;

	if (argc != 2)
		usage(argv[0]);
	if (sscanf(argv[1], "%d", &n) != 1)
		usage(argv[0]);
	if (n > 1)
		n--;
	if ((tid = calloc(n, sizeof(pthread_t *))) == NULL)
		err(1, "calloc()");
	if ((once = calloc(n, sizeof(int *))) == NULL)
		err(1, "calloc()");
	setonce(0);

	ps = getpagesize();
	p = mmap(NULL, n * ps, PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0);
	go = 0;
	pthread_mutex_init(&write_mutex, NULL);
	for (i = 0; i < n; i++) {
		arg = malloc(sizeof(int));
		*arg = i;
		if ((e = pthread_create(&tid[i], NULL, wr, (void *)arg)) != 0)
			errc(1, e, "pthread_create()");
	}
	go = 1;

	nb = 0;
	start = time(NULL);
	while (time(NULL) - start < 120) {
		if (mprotect(p, n * ps, PROT_READ|PROT_WRITE) == -1)
			err(1, "mprotect(PROT_READ)");
		done = 0;
		setonce(1);
		while (done != n)
			usleep(100);
		if (mprotect(p, n * ps, PROT_READ) == -1)
			err(1, "mprotect(PROT_READ)");
		nb++;
		usleep(100);
	}
	go = 0;
	for (i = 0; i < n; i++) {
		if ((e = pthread_join(tid[i], NULL)) != 0)
			errc(1, e, "pthread_join() in loop %d", i);
	}
	r = 0;
	for (i = 1; i < n; i++) {
		if (p[0] != p[i])
			r++;
	}
	if (r != 0) {
		fprintf(stderr, "%d loops.\n", nb);
		for (i = 0; i < n; i++)
			fprintf(stderr, "p[%3d] = %d\n", i, p[i]);
	}

	return (r);
}
EOF
mycc -o /tmp/$prog -Wall -Wextra -O0 -g /tmp/$prog.c -lpthread || exit 1

n=`sysctl -n hw.ncpu`
if [ $# -eq 1 ]; then
	echo $1 | grep -Eq '^[0-9]+$' && n=$1
fi
../testcases/swap/swap -t 2m > /dev/null &
sleep 10
/tmp/$prog $n; s=$?
pkill -9 swap
wait

rm -d /tmp/$prog /tmp/$prog.c
exit $s

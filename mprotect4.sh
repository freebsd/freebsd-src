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
/* N readers and 1 writer threaded test scenario */

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

static int go, n, ps;
static char *cp;
static volatile char v;

void *
rd(void *arg __unused)
{
	int i;

	while (go == 0)
		usleep(100);
	while (go == 1) {
		for (i = 0; i < n; i += ps) {
			v = cp[i];
		}
		pthread_yield();
	}
	return(NULL);
}

void
usage(char *prog) {
	fprintf(stderr, "Usage: %s <number of threads>\n", prog);
	_exit(1);
}

int
main(int argc, char *argv[])
{
	pthread_t *tid;
	time_t start;
	int e, i, nb;

	if (argc != 2)
		usage(argv[0]);
	if (sscanf(argv[1], "%d", &n) != 1)
		usage(argv[0]);
	if (n > 1)
		n--;
	if ((tid = calloc(n, sizeof(pthread_t *))) == NULL)
		err(1, "calloc()");

	ps = getpagesize();
	cp = mmap(NULL, n * ps, PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0);
	go = 0;
	for (i = 0; i < n; i++) {
		if ((e = pthread_create(&tid[i], NULL, rd, NULL)) != 0)
			errc(1, e, "pthread_create()");
	}
	go = 1;

	nb = 0;
	start = time(NULL);
	while (time(NULL) - start < 120) {
		for (i = 0; i < n; i += ps) {
			if (mprotect(&cp[i], ps, PROT_READ|PROT_WRITE) == -1)
				err(1, "mprotect(PROT_READ)");
			cp[i] = 1;
			if (mprotect(&cp[i], ps, PROT_READ) == -1)
				err(1, "mprotect(PROT_READ)");
			nb++;
		}
	}
	go = 0;
	for (i = 0; i < n; i++) {
		if ((e = pthread_join(tid[i], NULL)) != 0)
			errc(1, e, "pthread_join() in loop %d", i);
	}
	if (nb >= 0) {
#if defined(DEBUG)
		fprintf(stderr, "%d loops\n", nb);
#endif
		;
	}
}
EOF
mycc -o /tmp/$prog -Wall -Wextra -O0 /tmp/$prog.c -lpthread || exit 1

/tmp/$prog `sysctl -n hw.ncpu`; s=$?

rm -d /tmp/$prog /tmp/$prog.c
exit $s

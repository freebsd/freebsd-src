#!/bin/sh

#
# Copyright (c) 2025 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Seen:
# UID  PID PPID  C PRI NI   VSZ  RSS MWCHAN   STAT TT     TIME COMMAND
#   0 3730 3668 11  20  0 13596 2904 exithold DE+   0  1:59.68 ./kcmp

# Fixed by: 5b3e5c6ce3e5

. ../default.cfg

set -u
prog=$(basename "$0" .sh)
cat > /tmp/$prog.c <<EOF
#include <sys/types.h>

#include <err.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static void *
t1(void *data __unused)
{
	for (;;)
		pause();

	return (NULL);
}

int
main(void)
{
	pid_t p1, p2;
	pthread_t tid[2];
	time_t start;
	uintptr_t idx1, idx2;
	int r;

	if ((r = pthread_create(&tid[0], NULL, t1, NULL)) != 0)
		errc(1, r, "pthread_create");
	if ((r = pthread_create(&tid[1], NULL, t1, NULL)) != 0)
		errc(1, r, "pthread_create");

	start = time(NULL);
	while (time(NULL) - start < 60) {
		idx1 = idx2 = 0;
		p1 = arc4random() % 1000000;
		p2 = arc4random() % 1000000;
		kcmp(p1, p2, KCMP_VM, idx1, idx2);
	}
}
EOF
mycc -o /tmp/$prog -Wall -Wextra -O0 /tmp/$prog.c -lpthread || exit 1

/tmp/$prog

rm /tmp/$prog.c /tmp/$prog
exit 0

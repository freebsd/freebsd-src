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

# The test program calls fork(2) from a multi-threaded process.
# Test program stuck in uwrlck seen.
# Fixed in r266609.

# Note that program erroneously calls exit(3) and not _exit(2).

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > fork.c
mycc -o fork -Wall -Wextra -O2 -g fork.c -lpthread || exit 1

for i in `jot 100`; do
	/tmp/fork &
done
while ! pgrep -q fork; do
	sleep .2
done
for i in `jot 30`; do
	pgrep -q fork || break
	sleep 1
done
if pgrep -q fork; then
	echo FAIL
	exit 1
fi
wait

rm -f /tmp/fork /tmp/fork.c
exit 0
EOF

/*
 * Written by Love Hörnquist Åstrand <lha@NetBSD.org>, March 2003.
 * Public domain.
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static pid_t parent;
static int thread_survived = 0;

static void *
print_pid(void *arg __unused)
{
	sleep(3);

	thread_survived = 1;
	if (parent != getpid()) {
		exit(1);
	}
	return NULL;
}

int
main(void)
{
	int r;
	pthread_t p;
	pid_t fork_pid;

	parent = getpid();

	r = pthread_create(&p, NULL, print_pid, NULL);
	if (r != 0)
		errx(1, "r = %d", r);

	fork_pid = fork();
	if (fork_pid == -1)
		err(1, "fork");

	if (fork_pid) {
		int status;

		r = pthread_join(p, NULL);
		if (r != 0)
			errx(1, "r = %d", r);
		if (thread_survived == 0)
			errx(1, "thread did not survive in parent");

		waitpid(fork_pid, &status, 0);
		if (WIFEXITED(status) != 1)
			printf("WIFEXITED(status) = %d\n", WIFEXITED(status));
		if (WEXITSTATUS(status) != 0)
		printf("WEXITSTATUS(status) = %d\n", WEXITSTATUS(status));
	} else {
		sleep(5);
		exit(thread_survived ? 1 : 0);
	}
}

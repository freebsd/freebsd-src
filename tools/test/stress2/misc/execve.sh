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

# Test of execve(2) from a threaded program.
# "load: 5.40  cmd: bash 24517 [vmmaps] 2.45r 0.00u 0.00s 0% 3448k" seen.
# Fixed by r282708.

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > execve.c
mycc -o execve -Wall -Wextra -O2 execve.c -lpthread || exit 1
rm -f execve.c

daemon sh -c "(cd $here/../testcases/swap; ./swap -t 20m -i 20 -l 100)" > \
    /dev/null 2>&1
sleep `jot -r 1 1 9`
for i in `jot 2`; do
	/tmp/execve
done
while pgrep -q swap; do
	pkill -9 swap
done

rm -f /tmp/execve /tmp/execve.core
exit 0
EOF
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LOOPS 50
#define PARALLEL 50

volatile int go;

void *
texecve(void *arg __unused)
{
	char *cmdline[] = { "/usr/bin/true", NULL };

	while (go == 0)
		usleep(100);
        if (execve(cmdline[0], cmdline, NULL) == -1)
		err(1, "execve");

	return (NULL);
}

void
test(void)
{
	pthread_t tid[5];
	int i, rc;

	go = 0;

	for (i = 0; i < 5; i++) {
		if ((rc = pthread_create(&tid[i], NULL, texecve, NULL)) != 0)
			errc(1, rc, "texecve()");
	}

	usleep(arc4random() % 2000);
	go = 1;

	for (i = 0; i < 5; i++)
		if ((rc = pthread_join(tid[i], NULL)) != 0)
			errc(1, rc, "pthread_join(%d)", i);
	_exit(0);
}

int
main(void)
{
	struct rlimit rl;
	int i, j;

	rl.rlim_max = rl.rlim_cur = 0;
	if (setrlimit(RLIMIT_CORE, &rl) == -1)
		warn("setrlimit");

	for (i = 0; i < LOOPS; i++) {
		for (j = 0; j < PARALLEL; j++) {
			if (fork() == 0)
				test();
		}

		for (j = 0; j < PARALLEL; j++)
			wait(NULL);
	}

	return (0);
}

#!/bin/sh

#
# Copyright (c) 2016 EMC Corp.
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

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > context.c
mycc -o context -Wall -Wextra -O2 -g context.c || exit 1
rm -f context.c
[ -d $RUNDIR ] || mkdir -p $RUNDIR
cd $RUNDIR

daemon sh -c "(cd $here/../testcases/swap; ./swap -t 10m -i 20)" > \
    /dev/null 2>&1
for i in `jot 4`; do
	/tmp/context &
	pids="$pids $!"
done
s=0
for i in $pids; do
	wait $i
	[ $? -ne 0 ] && s=$((s + 1))
done
while pgrep -q swap; do
	pkill -9 swap
done
rm -f /tmp/context
exit $s
EOF
/*
 * Inspired by lmbench-3.0-a9/src/lat_ctx.c
 * Pass a token thru pipes to CHILDREN+1 processes in a circular list
 */

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define CHILDREN 64
#define RUNTIME 300

int fds[CHILDREN +1 ][2];
pid_t pid[CHILDREN];

void
handler(int s __unused)
{
	_exit(0);
}

int
main(void)
{
	time_t start;
	int i, j;
	int token;

	for (i = 0; i < CHILDREN + 1; i++) {
		if (pipe(fds[i]) == -1)
			err(1, "pipe");
	}

	signal(SIGHUP, handler);
	start = time(NULL);
	for (i = 0; i < CHILDREN; i++) {
		pid[i] = fork();
		if (pid[i] == -1) {
			perror("fork");
			exit(2);
		}

		if (pid[i] == 0) {	/* child */
			for (;;) {
				if (read(fds[i][0], &token, sizeof(token))
				    != sizeof(token))
					err(1, "read pipe 2");
				if (write(fds[i+1][1], &token, sizeof(token))
				    != sizeof(token))
					err(1, "write pipe 1");
			}
		}

	}	/* parent */

	for (j = 0; time(NULL) - start < RUNTIME; j++) {
		token = j;
		if (write(fds[0][1], &token, sizeof(token)) != sizeof(token))
			err(1, "write pipe 2");
		if (read(fds[CHILDREN][0], &token, sizeof(token))
		    != sizeof(token))
			err(1, "read pipe 1");
	}

	for (i = 0; i < CHILDREN; i++)
		kill(pid[i], SIGHUP);

        return (0);
}

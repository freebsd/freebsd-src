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

# Stress vm object collapse.

# "panic: backing_object 0xfffff800a018f420 was somehow re-referenced during
# collapse!" seen with uma_zalloc_arg fail point enabled.

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/pipe.c
mycc -o pipe -Wall -Wextra -O0 -g pipe.c || exit 1
rm -f pipe.c
cd $odir

daemon sh -c '(cd ../testcases/swap; ./swap -t 10m -i 20)' > /dev/null 2>&1
sleep 1
e=0
export e
start=`date '+%s'`
while [ $((`date '+%s'` - start)) -lt 300 ]; do
	for i in `jot $(sysctl -n hw.ncpu)`; do
		/tmp/pipe &
		pids="$pids $!"
	done
	for i in $pids; do
		wait $i
		[ $? -ne 0 ] && e=$((e + 1))
	done
	pids=""
	[ $e -ne 0 ] && break
done
while pgrep -q swap; do
	pkill -9 swap
done
rm -rf /tmp/pipe pipe.core
exit $e

EOF
#include <sys/wait.h>

#include <err.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define PIPES 64
#define RUNTIME 300

int
test(void)
{
	int c, e, status;
	int fds[PIPES][2];
	int i;

	for (i = 0; i < PIPES; i++) {
		if (pipe(fds[i]) == -1)
			err(1, "pipe");
	}
	c = e = 0;
	if (write(fds[0][1], &c, sizeof(c)) != sizeof(c))
		err(1, "pipe write");
	for (i = 0; i < PIPES; i++) {
		if (fork() == 0) {
			close(fds[i][1]);
			if (read(fds[i][0], &c, sizeof(c)) != sizeof(c))
				err(1, "pipe read");
#if defined(DEBUG)
			fprintf(stderr, "pid %d: i = %d: read %d\n", getpid(),
			    i, c);
#endif
			c++;
			if (i != PIPES - 1)
				if (write(fds[i + 1][1], &c, sizeof(c)) !=
				    sizeof(c))
					err(1, "pipe write");

			_exit(0);
		}
		close(fds[i][0]);
		close(fds[i][1]);
	}
	for (i = 0; i < PIPES; i++) {
		wait(&status);
		e += status == 0 ? 0 : 1;
	}

	return (e);
}

int
main(void)
{
	time_t start;
	int e;

	e = 0;
	start = time(NULL);
	while (time(NULL) - start < RUNTIME && e == 0)
		e = test();

	return (e);
}

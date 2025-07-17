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

. ../default.cfg

# Regression test for
# https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=203162

# FAIL
# 1001 82102   907   0  20  0 6484 2480 wait   S+    0  0:00,01 /bin/sh ./mkfifo3.sh
# 1001 82113 82102   0  52  0 3832 1604 fifoow I+    0  0:03,91 /tmp/mkfifo3

# Fixed in r288044.

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/mkfifo3.c
mycc -o mkfifo3 -Wall -Wextra -O0 -g mkfifo3.c || exit 1
rm -f mkfifo3.c
cd $odir

fifo=/tmp/mkfifo3.fifo
trap "rm -f $fifo /tmp/mkfifo3" EXIT INT

/tmp/mkfifo3 &

for i in `jot 12`; do
	pgrep -q mkfifo3 || break
	sleep 10
done
s=0
if pgrep -q mkfifo3; then
	s=1
	pgrep mkfifo3 | xargs ps -lp
	pkill mkfifo3
fi
wait

exit $s
EOF
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define ATIME 1
#define LOOPS 100000

char file[] = "/tmp/mkfifo3.fifo";

static void
hand(int i __unused) {	/* handler */
}

int
main(void)
{
	pid_t pid;
	struct sigaction sa;
	int fd, i, status;

	sa.sa_handler = hand;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGALRM, &sa, NULL) == -1)
		err(1, "sigaction");

	if (unlink(file) == -1)
		if (errno != ENOENT)
			err(1, "unlink(%s)", file);
	if (mkfifo(file, 0640) == -1)
		err(1, "mkfifo(%s)", file);

	for (i = 0; i < LOOPS; i++) {
		if ((pid = fork()) == 0) {
			ualarm(ATIME, 0);
			do {
				if((fd = open(file, O_RDONLY)) == -1)
					if (errno != EINTR)
						err(1, "open(%s, O_RDONLY @ %d)",
						    file, i);
			} while (fd == -1);
			if (close(fd) == -1)
				err(1, "close() in child");
			alarm(0);
			_exit(0);
		}
		ualarm(ATIME, 0);
		do {
			if ((fd = open(file, O_WRONLY)) == -1)
				if (errno != EINTR)
					err(1, "open(%s, O_WRONLY @ %d)",
					    file, i);
		} while (fd == -1);
		if (close(fd) == -1)
			err(1, "close() in parent");
		alarm(0);
		if (waitpid(pid, &status, 0) == -1)
			err(1, "wait");
	}

	if (unlink(file) == -1)
		err(1, "unlink(%s)", file);

	return (0);
}

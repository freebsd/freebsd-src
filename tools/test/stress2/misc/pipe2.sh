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

# pipe(2) test

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/pipe2.c
mycc -o pipe2 -Wall -Wextra -O0 -g pipe2.c || exit 1
rm -f pipe2.c

daemon sh -c "(cd $odir/../testcases/swap; ./swap -t 10m -i 20)" > \
    /dev/null 2>&1
sleep 10

su $testuser -c /tmp/pipe2
s=$?

while pgrep -q swap; do
	pkill -9 swap
done

rm -rf /tmp/pipe2
exit $s

EOF
#include <sys/param.h>
#include <sys/mman.h>

#include <machine/atomic.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

volatile u_int *share, *share2;

#define R1 1 /* sync start  */
#define R2 2 /* forks       */

#define PIPES 128
#define PARALLEL 32

static void
hand(int i __unused) {	/* handler */
	fprintf(stderr, "Timed out\n");
	_exit(1);
}

void
test(void)
{
	size_t len;
	int fds[2], r;
	int token;

	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	if (pipe(fds) == -1)
		err(1, "pipe");
	token = 0;
	write(fds[1], &token, sizeof(token));
	for (;;) {
		if (share[R2] >= PIPES)
			break;
		if ((r = fork()) == 0) {
			atomic_add_int(&share[R2], 1);
			if (read(fds[0], &token, sizeof(token)) != sizeof(token))
				err(1, "read");
			close(fds[0]);
			if (pipe(fds) == -1)
				err(1, "pipe");
			token++;
			if (write(fds[1], &token, sizeof(token)) != sizeof(token))
				err(1, "write");
		}
		if (r == -1)
			err(1, "fork()");
		if (r != 0)
			_exit(0);
	}

	if (share[R2] == PIPES) {
#if defined(DEBUG)
		if (read(fds[0], &token, sizeof(token)) != sizeof(token))
			err(1, "final read");
		fprintf(stderr, "FINAL read %d from %d\n", token, fds[0]);
#endif
		atomic_add_int(&share2[R1], 1);
	}
	_exit(0);
}

int
main(void)
{
	struct sigaction sa;
	size_t len;
	int i;

	len = PAGE_SIZE;
	if ((share2 = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGCHLD, &sa, 0) == -1)
		err(1, "sigaction");

	for (i = 0; i < PARALLEL; i++) {
		if (fork() == 0)
			test();
	}

	signal(SIGALRM, hand);
	alarm(300);
	while (share2[R1] != PARALLEL) {
		sleep(1);
#if defined(DEBUG)
		fprintf(stderr, "share2 = %d\n", share2[R1]);
#endif
	}

	return (0);
}

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

# 'WARNING: A device driver has set "memattr" inconsistently.' seen on
# console.
# https://people.freebsd.org/~pho/stress/log/mmap27.txt
# Fixed by r298891.

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > mmap27.c
mycc -o mmap27 -Wall -Wextra -g -O0 mmap27.c || exit 1
rm -f mmap27.c
cd $odir

daemon sh -c '(cd ../testcases/swap; ./swap -t 2m -i 20 -l 100)' > /dev/null 2>&1
sleep 2
/tmp/mmap27
while pgrep -q swap; do
	pkill -9 swap
done
rm -f ./mmap27 /tmp/mmap27.0* /tmp/mmap27
exit 0

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int fd;

#define ADRSPACE  (256 * 1024 * 1024 )
#define PARALLEL 64
#define RUNTIME 120
#define STARTADDR 0x50000000U

static void
work(void)
{
	size_t left, len;
	int i;
	char *p;
	volatile char val __unused;

	if ((fd = open("/dev/mem", O_RDWR)) == -1)
		err(1,"open()");

	p = (void *)STARTADDR + trunc_page(arc4random() % ADRSPACE);
	left = ADRSPACE - (size_t)p + STARTADDR;
	len = trunc_page(arc4random() % left) + PAGE_SIZE;

	if ((p = mmap(p, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) ==
			MAP_FAILED) {
		if (errno == ENOMEM)
			return;
		err(1, "mmap()");
	}

	for (i = 0; i < 100; i++)
		val = p[arc4random() % len];

	if (munmap(p, len) == -1)
		err(1, "munmap(%p)", p);

	_exit(0);
}

int
main(void)
{
	pid_t pids[PARALLEL];
	time_t start;
	int i, n;

	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		n = arc4random() % PARALLEL + 1;
		for (i = 0; i < n; i++) {
			if ((pids[i] = fork()) == 0)
				work();
		}

		for (i = 0; i < n; i++)
			if (waitpid(pids[i], NULL, 0) != pids[i])
				err(1, "waitpid(%d)", pids[i]);
	}
	close(fd);

	return (0);
}

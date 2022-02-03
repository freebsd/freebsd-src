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

# Core dump wedge
# Trimmed down version of mmap15.sh
# http://people.freebsd.org/~pho/stress/log/mmap15-2.txt
# Fixed in r272534 + r272535.

# Page fault seen: http://people.freebsd.org/~pho/stress/log/kostik733.txt
# Fixed in r274474.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > mmap22.c
mycc -o mmap22 -Wall -Wextra -O0 -g mmap22.c -lpthread || exit 1
rm -f mmap22.c

su $testuser -c /tmp/mmap22 &

sleep 300
while pgrep -q mmap22; do
	pkill -9 mmap22
        sleep 2
done
wait

rm -f /tmp/mmap22 /tmp/mmap22.core
exit 0
EOF
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MMAPS 25
#define PARALLEL 4
#define SIZ (64 * 1024 * 1024)

void *
tmmap(void *arg __unused)
{
	size_t len;
	void *p __unused;
	int i;

	len = SIZ;
	for (i = 0; i < MMAPS; i++)
		p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);
	fsync(fileno(stdout));

	return (NULL);
}

void
test(void)
{
	pthread_t tid;
	int i, rc;

	if ((rc = pthread_create(&tid, NULL, tmmap, NULL)) != 0)
		errc(1, rc, "test()");

	for (i = 0; i < 100; i++) {
		if (fork() == 0) {
			usleep(10000);
			_exit(0);
		}
		wait(NULL);
	}

	raise(SIGSEGV);

	if ((rc = pthread_join(tid, NULL)) != 0)
		errc(1, rc, "pthread_join(%d)", i);
	_exit(0);
}

int
main(void)
{
	int i;

	for (i = 0; i < PARALLEL; i++) {
		if (fork() == 0)
			test();
	}

	for (i = 0; i < PARALLEL; i++)
		wait(NULL);

	return (0);
}

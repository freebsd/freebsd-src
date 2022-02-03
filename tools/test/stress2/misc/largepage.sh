#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2020 Peter Holm <pho@FreeBSD.org>
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

# Test scenario for of non-transparent superpages.

# No problems seen.

. ../default.cfg
[ `uname -p` = "i386" ] && exit 0
[ -z "`sysctl -i vm.largepages`" ] && exit 0

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > largepage.c
mycc -o largepage -Wall -Wextra -g -O0 largepage.c || exit 1
rm -f largepage.c
cd $odir

/tmp/largepage
s=$?

for path in `posixshmcontrol ls | grep largepage | awk '{print $NF}'`; do
	echo "posixshmcontrol rm $path"
	posixshmcontrol rm $path
done
rm -f ./largepage /tmp/largepage.0* /tmp/largepage
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

static size_t ps[MAXPAGESIZES];
static int fd;

#define PARALLEL 4
#define RUNTIME 60

static void
work(int idx)
{
	size_t len;
	int i,r;
	char *p;
	volatile char val __unused;
	char path[PATH_MAX];

	len = ps[idx];
	sprintf(path, "/tmp/largepage.%06d", getpid());
	if ((fd = shm_create_largepage(path, O_CREAT | O_RDWR, idx,
	    SHM_LARGEPAGE_ALLOC_DEFAULT, 0600)) == -1)
		err(1,"shm_create_largepage(%zu)", len);

	for (i = 0; i < 100; i++) {
		r = ftruncate(fd, len);
		if (r == 0)
			break;
		usleep(200000);
	}
	if (r == -1) {
		shm_unlink(path);
		err(1, "ftruncate(%zu)", len);
	}
	close(fd);
	if ((fd = shm_open(path, O_RDWR, 0)) == -1)
		err(1, "shm_open()");

	if ((p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) ==
			MAP_FAILED) {
		if (errno == ENOMEM)
			return;
		err(1, "mmap()");
	}

	val = p[arc4random() % len];

	close(fd);
	if (munmap(p, len) == -1)
		err(1, "munmap(%p)", p);
	if (shm_unlink(path) == -1)
		err(1, "shm_unlink()");

	_exit(0);
}

int
main(void)
{
	pid_t pids[PARALLEL];
	time_t start;
	int e, i, n, nps, status;

	nps = getpagesizes(ps, MAXPAGESIZES);
	e = 0;
	n = 1;
	start = time(NULL);
	while (time(NULL) - start < RUNTIME && e == 0) {
		for (i = 0; i < PARALLEL; i++) {
			if ((pids[i] = fork()) == 0)
				work(n);
		}

		for (i = 0; i < PARALLEL; i++) {
			if (waitpid(pids[i], &status, 0) != pids[i])
				err(1, "waitpid(%d)", pids[i]);
			if (status != 0) e = 1;
		}
		n++;
		n = n % nps;
		if (n == 0) /* skip 4k pages */
			n = 1;
	}
	close(fd);

	return (e);
}

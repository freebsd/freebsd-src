#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Dell EMC Isilon
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

# "panic: Most recently used by tty" seen.
# Reported by syzbot+c9b6206303bf47bac87e@syzkaller.appspotmail.com
# Fixed by r349733

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
kldstat -v | grep -q pty || { kldload pty || exit 0; }

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > pts3.c
mycc -o pts3 -Wall -Wextra -O2 pts3.c || exit 1
rm -f pts3.c

/tmp/pts3; s=$?

rm -f /tmp/pts3
exit $s
EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <machine/atomic.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static volatile u_int *share;
#define SYNC 0

int
main(void)
{
	pid_t p, pid;
	size_t len;
	time_t start;
        int fd;
	char path[128];

	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	p = getpid();
        sprintf(path, "/dev/ptmx");
	start = time(NULL);
	while (time(NULL) - start < 60) {
		share[SYNC] = 0;
		if ((fd = open(path, O_RDWR)) == -1)
			err(1,"open()");
		if ((pid = fork()) == 0) {
			while (share[SYNC] == 0)
				;
			_exit(0);
		}
		share[SYNC] = 1;
		if (fcntl(fd, F_SETOWN, p) == -1)
			warn("fcntl()");
		close(fd);
		if (waitpid(pid, NULL, 0) != pid)
			err(1, "waitpid()");
	}

        return (0);
}

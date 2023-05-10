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

# markj@ wrote:
# I found a kernel bug that caused init to consume 100% CPU if the
# following steps occurred:
# - process A forks and creates process B
# - process C ptrace attaches to process B
# - process A exits
# - process C detaches from process B
# - process B exits
#
# Process B gets reparented to init, and the bug causes wait() to return
# an error each time init attempts to reap process B.

. ../default.cfg
cd /tmp
cat > ptrace11.c <<EOF
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

#include <machine/atomic.h>

#include <err.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static volatile u_int *share;
#define SYNC 0

int
main(void)
{
	pid_t pida, pidb;
	size_t len;
	int status;

	setproctitle("A");
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	pida = fork();
	if (pida < 0)
		err(1, "fork");
	if (pida == 0) {
		setproctitle("B");
		while (share[SYNC] != 2)
			usleep(10);
		_exit(0);
	} else {
		sleep(1);
		pidb = fork();
		if (pidb < 0)
			err(1, "fork");
		if (pidb == 0) {
			setproctitle("C");
			if (ptrace(PT_ATTACH, pida, NULL, 0) != 0)
				err(1, "ptrace(PT_ATTACH)");
			wait4(pida, &status, 0, NULL);
			share[SYNC] = 1; /* A to exit */
			usleep(1000);
			if (ptrace(PT_DETACH, pida, NULL, 0) != 0)
				err(1, "ptrace(PT_DETACH)");
			share[SYNC] = 2; /* B to exit */
			_exit(0);
		} else {
			while (share[SYNC] != 1)
				usleep(10);
			_exit(0);
		}
	}

	return (0);
}
EOF

mycc -o ptrace11 -Wall -Wextra -O2 -g ptrace11.c || exit 1
rm ptrace11.c
old=`ps auxwwl | grep -v grep | grep "<defunct>" | wc -l`

./ptrace11

new=`ps auxwwl | grep -v grep | grep "<defunct>" | wc -l`
if [ $old -ne $new ]; then
	ps auxwwl | sed -n '1p;/<defunct>/p'
	echo FAIL
	s=1
fi

rm -f ptrace11
exit $s

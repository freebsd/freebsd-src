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

# truss(1) of a multithreaded program.

# FAIL
#   0 90968 90933   0  52  0 6028    0 wait   IW+ 1  0:00,00 truss /tmp/ttruss
#   0 90970 90968   0  52  0 6436 1560 uwrlck IX+ 1  0:00,00 /tmp/ttruss
# $ procstat -k 90970
#   PID    TID COMM             TDNAME           KSTACK
# 90970 101244 ttruss           -                mi_switch sleepq_switch
#     sleepq_catch_signals sleepq_wait_sig _sleep umtxq_sleep
#     do_rw_wrlock __umtx_op_rw_wrlock syscall Xint0x80_syscall
# $

# Only seen during testing of a WiP ptrace(2) patch.

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/ttruss.c
mycc -o ttruss -Wall -Wextra -O0 -g ttruss.c -lpthread || exit 1
rm -f ttruss.c

# VM pressure is not mandatory, but shortens the time to failure.
daemon sh -c \
    "(cd $odir/../testcases/swap; ./swap -t 6m -i 20 -k -l 100)" > \
    /dev/null
sleep .5
for i in `jot 30`; do
	truss /tmp/ttruss 10 > /dev/null 2>&1 &
	sleep 11
	if ps -lx | grep -v grep | grep -q uwrlck; then
		echo FAIL
		ps -lH | egrep -v "grep|truss.sh" | grep truss
		while pkill -9 swap; do
			:
		done
		exit 1
	fi
	wait
done
while pkill -9 swap; do
	:
done
sleep 2
if pgrep -q ttruss; then
	echo FAIL
	ps -lxH | grep -v grep | grep ttruss
	s=1
fi

[ -f /tmp/truss.core ] && { ls -l /tmp/truss.core; s=1; }
rm -rf /tmp/ttruss /tmp/ttruss.core
exit $s

EOF
#include <sys/param.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define THREADS 16

static void *
t1(void *data __unused)
{
	return (NULL);
}

int
main(int argc, char *argv[])
{
	pthread_t tid[THREADS];
	time_t start;
	int i, rc, runtime;

	if (argc != 2)
		errx(1, "Usage: %s <runtime>", argv[0]);
	runtime = atoi(argv[1]);
	start = time(NULL);
	while ((time(NULL) - start) < runtime) {
		for (i = 0; i < THREADS; i++) {
			if ((rc = pthread_create(&tid[i], NULL, t1, NULL)) !=
			    0)
				errc(1, rc, "pthread_create");
		}

		for (i = 0; i < THREADS; i++) {
			if ((rc = pthread_join(tid[i], NULL)) != 0)
				errc(1, rc, "pthread_join");
		}
	}

	return (0);
}

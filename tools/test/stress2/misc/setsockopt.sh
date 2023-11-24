#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
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

# setsockopt() fuzz test scenario inspired by syzkaller.

# "panic: mtx_lock() of spin mutex (null) @
# ../../../dev/hyperv/hvsock/hv_sock.c:519" seen.
# https://people.freebsd.org/~pho/stress/log/setsockopt.txt
# Introduced by r361275
# Fixed by r361360

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/setsockopt.c
mycc -o setsockopt -Wall -Wextra -O0 -g setsockopt.c || exit 1
rm -f setsockopt.c

daemon sh -c "(cd $odir/../testcases/swap; ./swap -t 5m)" > /dev/null 2>&1
/tmp/setsockopt
s=$?
while pgrep -q swap; do
	pkill -9 swap
done
rm -rf /tmp/setsockopt /tmp/setsockopt.c
exit $s
EOF
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PARALLEL 128
#define RUNTIME (3 * 60)

static int port;

static void
test(int idx) {
	struct hostent *hostent;
	struct sockaddr_in inetaddr;
	int i, j, r;
	int tcpsock, on;

	on = 1;
	for (i = 1; i < 5; i++) {
		for (j = 0; j < 10000; j++) {
			if ((tcpsock = socket(arc4random() % 64, arc4random() % 10, arc4random() % 10)) != -1)
				break;
		}
		if (tcpsock == -1)
			_exit(0);

		/*
		if (setsockopt(tcpsock,
		    SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
			err(1, "setsockopt(), %s:%d", __FILE__, __LINE__);
		*/
		for (j = 0; j < 10000; j++) {
			r = setsockopt(tcpsock, arc4random(), arc4random(), (char *)&on, sizeof(on));
			if (r != -1)
				break;
		}

		hostent = gethostbyname ("localhost");
		r = -1;
		for (j = 0; j < 10000 && r != 0; j++) {
			bzero((char *) &inetaddr, sizeof(inetaddr));
			memcpy (&inetaddr.sin_addr.s_addr, hostent->h_addr,
				sizeof (struct in_addr));

//			inetaddr.sin_family = AF_INET;
			inetaddr.sin_family = arc4random() % 128;
//			inetaddr.sin_addr.s_addr = INADDR_ANY;
			inetaddr.sin_addr.s_addr = arc4random();
			inetaddr.sin_port = htons(port + idx);
			inetaddr.sin_len = sizeof(inetaddr);

			alarm(1);
			r = connect(tcpsock, (struct sockaddr *) &inetaddr,
				sizeof(inetaddr));
			alarm(0);
		}
		usleep(1000);
		close(tcpsock);
	}
	write(tcpsock, "a", 1);
	_exit(0);
}

int
main(void)
{
	time_t start;
	int i, pids[PARALLEL], status;

	port = 77665;
	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME) {
		for (i = 0; i < PARALLEL; i++) {
			if ((pids[i] = fork()) == 0)
				test(i);
		}
		for (i = 0; i < PARALLEL; i++) {
			if (waitpid(pids[i], &status, 0) == -1)
				err(1, "waitpid(%d)", pids[i]);
		}
	}

	return (0);
}

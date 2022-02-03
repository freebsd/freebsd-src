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

# No problems seen
# Stand alone version of testcases/tcp

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/tcp3.c
mycc -o tcp3 -Wall -Wextra -O0 -g tcp3.c || exit 1
rm -f tcp3.c

daemon sh -c "(cd $odir/../testcases/swap; ./swap -t 5m)" > /dev/null 2>&1
timeout 2m /tmp/tcp3
s=$?
while pgrep -q swap; do
	pkill -9 swap
done

if pgrep -q tcp3; then
	echo FAIL
	pgrep tcp3 | while read pid; do
		ps -lp$pid
		procstat -kk $pid
	done
	exit 1
fi
rm -rf /tmp/tcp3 /tmp/tcp3.c
exit $s

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <machine/atomic.h>

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

#define NB (400 * 1024 * 1024)
#define SYNC 0
#define PARALLEL 8
#define RUNTIME (1 * 60)

static sig_atomic_t done_testing;
static volatile u_int *share;
static int bufsize;
static int port;

static void
alarm_handler(int i __unused)
{

	done_testing++;
}

static void
exit_handler(int i __unused)
{

	_exit(1);
}

static void
callcleanup(void)
{
}

static int
random_int(int mi, int ma)
{
	return (arc4random()  % (ma - mi + 1) + mi);
}

static void
reader(void) {
	struct sockaddr_in inetaddr, inetpeer;
	socklen_t len;
	int n, *buf;
	int on;
	int tcpsock, msgsock;

	on = 1;
	if ((tcpsock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		err(1, "socket(), %s:%d", __FILE__, __LINE__);

	if (setsockopt(tcpsock,
	    SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
		err(1, "setsockopt(), %s:%d", __FILE__, __LINE__);

	inetaddr.sin_family = AF_INET;
	inetaddr.sin_addr.s_addr = INADDR_ANY;
	inetaddr.sin_port = htons(port);
	inetaddr.sin_len = sizeof(inetaddr);

	if (bind(tcpsock,
	    (struct sockaddr *)&inetaddr, sizeof (inetaddr)) < 0)
		err(1, "bind(), %s:%d", __FILE__, __LINE__);

	if (listen(tcpsock, 5) < 0)
		err(1, "listen(), %s:%d", __FILE__, __LINE__);

	if (random_int(1,100) > 60)
		sleep(6);

	len = sizeof(inetpeer);
	if ((msgsock = accept(tcpsock,
	    (struct sockaddr *)&inetpeer, &len)) < 0)
		err(1, "accept(), %s:%d", __FILE__, __LINE__);

	if ((buf = malloc(bufsize)) == NULL)
			err(1, "malloc(%d), %s:%d", bufsize, __FILE__,
			    __LINE__);
	while (done_testing == 0) {
		if ((n = read(msgsock, buf, bufsize)) < 0)
			err(1, "read(), %s:%d", __FILE__, __LINE__);
		if (n == 0) break;
	}
	close(msgsock);
	return;
}

static void
writer(void) {
	struct hostent *hostent;
	struct sockaddr_in inetaddr;
	int i, *buf, r;
	int tcpsock, on;

	on = 1;
	for (i = 1; i < 5; i++) {
		if ((tcpsock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
			err(1, "socket(), %s:%d", __FILE__, __LINE__);

		if (setsockopt(tcpsock,
		    SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
			err(1, "setsockopt(), %s:%d", __FILE__, __LINE__);

		hostent = gethostbyname ("localhost");
		bzero((char *) &inetaddr, sizeof(inetaddr));
		memcpy (&inetaddr.sin_addr.s_addr, hostent->h_addr,
			sizeof (struct in_addr));

		inetaddr.sin_family = AF_INET;
		inetaddr.sin_port = htons(port);
		inetaddr.sin_len = sizeof(inetaddr);

		r = connect(tcpsock, (struct sockaddr *) &inetaddr,
			sizeof(inetaddr));
		if (r == 0)
			break;
		sleep(1);
		close(tcpsock);
	}
	if (r < 0)
		err(1, "connect(), %s:%d", __FILE__, __LINE__);

	if ((buf = malloc(bufsize)) == NULL)
			err(1, "malloc(%d), %s:%d", bufsize, __FILE__,
			    __LINE__);
	for (i = 0; i < bufsize / (int)sizeof(int); i++)
		buf[i] = i;

	for (;;) {
		for (i = 0; i < NB; i+= bufsize) {
			if (write(tcpsock, buf, bufsize) < 0) {
				if (errno == EPIPE)
					return;
				if (errno != ECONNRESET)
					err(1, "write(%d), %s:%d", tcpsock,
					    __FILE__, __LINE__);
				_exit(EXIT_SUCCESS);
			}
		}
	}
	return;
}

static int
test2(void)
{
	pid_t pid;

	if ((pid = fork()) == 0) {
		writer();
		_exit(EXIT_SUCCESS);

	} else if (pid > 0) {
		reader();
		kill(pid, SIGINT);
		waitpid(pid, NULL, 0);
	} else
		err(1, "fork(), %s:%d",  __FILE__, __LINE__);

	return (0);
}

static void
test(int nb)
{
	atomic_add_int(&share[SYNC], 1);
	while (share[SYNC] != PARALLEL)
		;

	port = 12340 + nb;
	bufsize = 2 << random_int(1, 12);
	signal(SIGALRM, alarm_handler);
	signal(SIGINT, exit_handler);
	atexit(callcleanup);

	alarm(2);
	test2();

	_exit(0);
}

int
main(void)
{
	size_t len;
	time_t start;
	int e, i, pids[PARALLEL], status;

	e = 0;
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME && e == 0) {
		share[SYNC] = 0;
		for (i = 0; i < PARALLEL; i++) {
			if ((pids[i] = fork()) == 0)
				test(i);
		}
		for (i = 0; i < PARALLEL; i++) {
			if (waitpid(pids[i], &status, 0) == -1)
				err(1, "waitpid(%d)", pids[i]);
			e += status == 0 ? 0 : 1;
		}
	}

	return (e);
}

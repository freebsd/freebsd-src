#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2018 Dell EMC Isilon
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

# "panic: in_pcbrele_wlocked: refcount 0" seen in WiP kernel code.

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

cd /tmp

cat > /tmp/tcp4.c <<EOF
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define NB (400 * 1024 * 1024)
#define RUNTIME (16 * 60)

static int port;
static int bufsize;

static int
random_int(int mi, int ma)
{
        return (arc4random()  % (ma - mi + 1) + mi);
}

static void
reader(void) {
	socklen_t len;
	struct sockaddr_in inetaddr, inetpeer;
	int tcpsock, msgsock ,on, n, *buf;

	on = 1;
	setproctitle("%s", __func__);
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

	if ((random_int(1,100) > 60)) {
		usleep(random_int(1000000,1000000) * 60);
	}

	len = sizeof(inetpeer);
	if ((msgsock = accept(tcpsock,
	    (struct sockaddr *)&inetpeer, &len)) < 0)
		err(1, "accept(), %s:%d", __FILE__, __LINE__);

	if ((buf = malloc(bufsize)) == NULL)
			err(1, "malloc(%d), %s:%d", bufsize, __FILE__, __LINE__);
	for (;;) {
		if ((n = read(msgsock, buf, bufsize)) < 0)
			err(1, "read(), %s:%d", __FILE__, __LINE__);
		if (n == 0) break;
	}
	close(msgsock);
	return;
}

static void
writer(void) {
	struct sockaddr_in inetaddr;
	struct hostent *hostent;
	time_t start;
	int i, *buf, r;
	int tcpsock, on;

	on = 1;
	setproctitle("%s", __func__);
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
			err(1, "malloc(%d), %s:%d", bufsize, __FILE__, __LINE__);
	for (i = 0; i < bufsize / (int)sizeof(int); i++)
		buf[i] = i;

	start = time(NULL);
	while (time(NULL) - start < 60) {
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

int
setup(int nb)
{
	port = 12340 + nb;
	bufsize = 2 << random_int(1, 12);
	return (0);
}

int
test(void)
{
	pid_t pid;

	sleep(1);
	if ((pid = fork()) == 0) {
		alarm(180);
		writer();
		_exit(EXIT_SUCCESS);

	} else if (pid > 0) {
		alarm(180);
		reader();
		kill(pid, SIGINT);
		if (waitpid(pid, NULL, 0) != pid)
			err(1, "waitpid(%d)", pid);
	} else
		err(1, "fork(), %s:%d",  __FILE__, __LINE__);

	return (0);
}

int
main(int argc, char *argv[])
{
	pid_t *pid;
	time_t start;
	int i, n;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <threads>\n", argv[0]);
		exit(1);
	}
	n = atoi(argv[1]);
	pid = calloc(sizeof(pid_t *), n);

	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		for (i = 0; i < n; i++) {
			if ((pid[i] = fork()) == 0) {
				setup(i);
				test();
				_exit(0);
			}
		}
		for (i = 0; i < n; i++)
			if (waitpid(pid[i], NULL, 0) != pid[i])
				err(1, "waitpid(%d) l%d", pid[i], __LINE__);
		fprintf(stderr, "*** Done loop\n");
	}

	return (0);
}
EOF

mycc -o tcp4 -Wall -Wextra -O2 tcp4.c || exit 1
rm tcp4.c

n=`sysctl -n kern.maxprocperuid`
n=$((n / 2 / 10 * 8))
./tcp4 $n > /dev/null 2>&1 &
sleep 30
while pgrep -q tcp4; do
	pkill tcp4
done
wait
rm -f tcp4
exit 0

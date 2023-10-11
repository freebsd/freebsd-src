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

# Stress the jumbo mbuf allocation
# vmstat -z | sed -n '1p;/jumbo/p'

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > jumbo.c
mycc -o jumbo -Wall -Wextra -O2 jumbo.c || exit 1
rm -f jumbo.c

log=/tmp/mbuf.log
nb=80
host=localhost
[ $# -eq 1 ] && host=$1
(
	for i in `jot $nb 0`; do
		/tmp/jumbo $((12345 + $i)) &
	done
	sleep 30
	for i in `jot $nb 0`; do
		/tmp/jumbo $host $((12345 + $i)) &
	done
	for i in `jot $nb 0`; do
		wait
	done
) > $log 2>&1

rm -f /tmp/jumbo
grep -q FAIL $log && { cat $log 1>&2; exit 1; }
rm -f $log
exit 0
EOF
#include <sys/param.h>
#include <sys/socket.h>
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
#include <unistd.h>

#define MX (1024 * 1024)
#define RETRIES 5
#define TIMEOUT 1200

char *host;
int loops, port, server;

void
ahandler(int s __unused)
{
	if (server)
		fprintf(stderr, "FAIL Server timed out after %d loops.\n",
		    loops);
	else
		fprintf(stderr, "FAIL Client timed out after %d loops.\n",
		    loops);
	_exit(0);
}

static void
reader(void) {
	socklen_t len;
	struct sockaddr_in inetaddr, inetpeer;
	int *buf, i, n, msgsock, on, tcpsock;

	setproctitle("reader - init");
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

	signal(SIGALRM, ahandler);
	alarm(TIMEOUT);
	if (bind(tcpsock,
	    (struct sockaddr *)&inetaddr, sizeof (inetaddr)) < 0)
		err(1, "bind(), %s:%d", __FILE__, __LINE__);

	if (listen(tcpsock, 5) < 0)
		err(1, "listen(), %s:%d", __FILE__, __LINE__);

	len = sizeof(inetpeer);
	if ((msgsock = accept(tcpsock,
	    (struct sockaddr *)&inetpeer, &len)) < 0)
		err(1, "accept(), %s:%d", __FILE__, __LINE__);

	if ((buf = malloc(MX)) == NULL)
			err(1, "malloc(%d), %s:%d", MX, __FILE__, __LINE__);
	setproctitle("reader");
	for (i = sysconf(_SC_PAGESIZE); i < MX; i += 1024) {
		alarm(TIMEOUT);
		if ((n = recvfrom(msgsock, buf, i, MSG_WAITALL, NULL,
		    NULL)) < 0) {
			if (errno == EAGAIN)
				continue;
			err(1, "read(), %s:%d", __FILE__, __LINE__);
		}
		if (n == 0)
			break;

		loops++;
	}
	close(msgsock);
	_exit(0);
}

static void
writer(void) {
	struct sockaddr_in inetaddr;
	struct hostent *hostent;
	int i, on, r, tcpsock;
	char *line;

	setproctitle("writer - init");
	signal(SIGALRM, ahandler);
	alarm(TIMEOUT);
	on = 1;
	for (i = 0; i < RETRIES; i++) {
		if ((tcpsock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
			err(1, "socket(), %s:%d", __FILE__, __LINE__);

		if (setsockopt(tcpsock,
		    SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
			err(1, "setsockopt(), %s:%d", __FILE__, __LINE__);

		if ((hostent = gethostbyname (host)) == NULL)
			err(1, "gethostbyname(%s)", host);
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
		err(1, "connect(%s, %d), %s:%d", host, port, __FILE__,
		    __LINE__);

	setproctitle("writer");
	if ((line = malloc(MX)) == NULL)
			err(1, "malloc(%d), %s:%d", MX, __FILE__, __LINE__);
	alarm(TIMEOUT);
	for (i = sysconf(_SC_PAGESIZE); i < MX; i += 1024) {
		if (write(tcpsock, line, i) < 0)
			err(1, "socket write(). %s:%d", __FILE__, __LINE__);
		loops++;
	}
	close(tcpsock);

	return;
}

int
main(int argc, char **argv)
{

	if (argc == 2) {
		server = 1;
		port = atoi(argv[1]);
		reader();
	} else if (argc == 3) {
		host = argv[1];
		port = atoi(argv[2]);
		writer();
	} else
		errx(1, "Usage: %s {<host>} <port number>\n", argv[0]);

	return (0);
}

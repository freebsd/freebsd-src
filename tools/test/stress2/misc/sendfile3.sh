#!/bin/sh

#
# Copyright (c) 2011 Peter Holm <pho@FreeBSD.org>
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

# Test scenario for sendfile deadlock (processes stuck in sfbufa)

# Variation of sendfile.sh
# kern.ipc.nsfbufs should be low for this test

. ../default.cfg

odir=`pwd`

cd /tmp
sed '1,/^EOF/d' < $odir/$0 > sendfile3.c
mycc -o sendfile3 -Wall -Wextra sendfile3.c
rm -f sendfile3.c
[ -d "$RUNDIR" ] || mkdir -p $RUNDIR
cd $RUNDIR

in=inputFile
out=outputFile
parallel=20

for i in 50m 100m; do
	rm -f $in
	dd if=/dev/random of=$in bs=$i count=1 status=none
	for j in `jot $parallel`; do
		rm -f ${out}$j
		/tmp/sendfile3 $in ${out}$j 1234$j &
	done
	wait
	for j in `jot $parallel`; do
		rm -f ${out}$j
	done
done
rm -f $in /tmp/sendfile3
exit
EOF
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <err.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int port;
char *inputFile;
char *outputFile;
int bufsize = 4096;

static void
reader(void) {
	int tcpsock, msgsock;
	int on;
	socklen_t len;
	struct sockaddr_in inetaddr, inetpeer;
	int n, *buf, fd;

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

	len = sizeof(inetpeer);
	if ((msgsock = accept(tcpsock,
	    (struct sockaddr *)&inetpeer, &len)) < 0)
		err(1, "accept(), %s:%d", __FILE__, __LINE__);

	if ((buf = malloc(bufsize)) == NULL)
		err(1, "malloc(%d), %s:%d", bufsize, __FILE__, __LINE__);

	if ((fd = open(outputFile, O_RDWR | O_CREAT | O_TRUNC, 0640)) == -1)
		err(1, "open(%s)", outputFile);

	for (;;) {
		if ((n = read(msgsock, buf, bufsize)) < 0)
			err(1, "read(), %s:%d", __FILE__, __LINE__);
		if (n == 0) break;

		if ((write(fd, buf, n)) != n)
			err(1, "write");
	}
	close(msgsock);
	close(fd);
	return;
}

static void
writer(void) {
	int tcpsock, on;
	struct sockaddr_in inetaddr;
	struct hostent *hostent;
	struct stat statb;
	int i, r, fd;
	off_t off = 0;

	on = 1;
	for (i = 0; i < 5; i++) {
		if ((tcpsock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
			err(1, "socket(), %s:%d", __FILE__, __LINE__);

		if (setsockopt(tcpsock,
		    SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
			err(1, "setsockopt(), %s:%d", __FILE__, __LINE__);

		bzero((char *) &inetaddr, sizeof(inetaddr));
		hostent = gethostbyname ("localhost");
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

        if (stat(inputFile, &statb) != 0)
                err(1, "stat(%s)", inputFile);

	if ((fd = open(inputFile, O_RDONLY)) == -1)
		err(1, "open(%s)", inputFile);

	if (sendfile(fd, tcpsock, 0, statb.st_size, NULL, &off, 0) == -1)
		err(1, "sendfile");
}

int
main(int argc, char **argv)
{
	pid_t pid;

	if (argc != 4) {
		fprintf(stderr, "Usage: %s <inputFile outputFile portNumber\n", argv[0]);
			return (1);
	}
	inputFile = argv[1];
	outputFile = argv[2];
	port = atoi(argv[3]);

	if ((pid = fork()) == 0) {
		writer();
		exit(EXIT_SUCCESS);
	} else if (pid > 0) {
		reader();
		kill(pid, SIGINT);
		waitpid(pid, NULL, 0);
	} else
		err(1, "fork(), %s:%d",  __FILE__, __LINE__);

	return (0);
}

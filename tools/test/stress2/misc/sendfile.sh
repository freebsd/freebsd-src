#!/bin/sh

#
# Copyright (c) 2009 Peter Holm <pho@FreeBSD.org>
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

# Test scenario for sendfile livelock seen on 7.2-STABLE for non SMP

# Scenario by kib@

. ../default.cfg

odir=`pwd`

cd /tmp
sed '1,/^EOF/d' < $odir/$0 > sendfile.c
mycc -o sendfile -Wall -O0 sendfile.c -pthread
rm -f sendfile.c
[ -d "$RUNDIR" ] || mkdir -p $RUNDIR
cd $RUNDIR

in=inputFile
out=outputFile

for i in 1 2 3 4 8 16 1k 2k 3k 4k 5k 1m 2m 3m 4m 5m ; do
	rm -f $in $out
	dd if=/dev/random of=$in bs=$i count=1 status=none
	/tmp/sendfile $in $out 12345
	cmp $in $out || { echo FAIL; ls -l $in $out; }
	rm -f $in $out
done
rm -f /tmp/sendfile
exit
EOF
#include <err.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
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
	int n, t __unused, *buf, fd;

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

	t = 0;
	if ((buf = malloc(bufsize)) == NULL)
		err(1, "malloc(%d), %s:%d", bufsize, __FILE__, __LINE__);

	if ((fd = open(outputFile, O_RDWR | O_CREAT | O_TRUNC, 0640)) == -1)
		err(1, "open(%s)", outputFile);

	for (;;) {
		if ((n = read(msgsock, buf, bufsize)) < 0)
			err(1, "read(), %s:%d", __FILE__, __LINE__);
		t += n;
		if (n == 0)
			break;

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
#if 1
	size_t size;
#endif

	on = 1;
	for (i = 1; i < 5; i++) {
		if ((tcpsock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
			err(1, "socket(), %s:%d", __FILE__, __LINE__);

		if (setsockopt(tcpsock,
		    SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
			err(1, "setsockopt(), %s:%d", __FILE__, __LINE__);

#if 1		/* livelock trigger */
		size = getpagesize() -4;
		if (setsockopt(tcpsock, SOL_SOCKET, SO_SNDBUF, (void *)&size,
		    sizeof(size)) < 0)
			err(1, "setsockopt(SO_SNDBUF), %s:%d",
			    __FILE__, __LINE__);
#endif

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

	return;
}

int
main(int argc, char **argv)
{
	pid_t pid;

	if (argc != 4) {
		fprintf(stderr, "Usage: %s <inputFile outputFile portNumber\n",
		    argv[0]);
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
	} else
		err(1, "fork(), %s:%d",  __FILE__, __LINE__);

	return (0);
}

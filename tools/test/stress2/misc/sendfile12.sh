#!/bin/sh

#
# Copyright (c) 2017 Dell EMC Isilon
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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# Test scenario suggestion by alc@

# sendfile(2) using SF_NOCACHE and a mmap'ed file with touched pages.
# "panic: vm_page_free_toq: freeing mapped page 0xc49e53bc" seen:
# https://people.freebsd.org/~pho/stress/log/sendfile12.txt

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > sendfile12.c
mycc -o sendfile12 -Wall -O0 sendfile12.c -pthread || exit 1
rm -f sendfile12.c

set -e
mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags /dev/md$mdstart > /dev/null

mount /dev/md${mdstart} $mntpoint
chmod 777 $mntpoint
set +e

cd $mntpoint
dd if=/dev/zero of=file bs=1m count=512 status=none
(cd $odir/../testcases/swap; ./swap -t 5m -i 20 -h -l 100) > /dev/null &
sleep 5
/tmp/sendfile12 file output 12345; s=$?
cd $odir
while pkill swap; do
	sleep 1
done
wait

while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
checkfs /dev/md${mdstart} || s=$?
mdconfig -d -u $mdstart
rm -f /tmp/sendfile12
exit $s
EOF
#include <sys/param.h>
#include <sys/mman.h>
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

#define BUFSIZE 8192

static int port;
static char *inputFile;
static char *outputFile;

static void
reader(void) {
	struct sockaddr_in inetaddr, inetpeer;
	socklen_t len;
	int tcpsock, msgsock;
	int *buf, fd, n, on, t __unused;

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
	if ((buf = malloc(BUFSIZE)) == NULL)
		err(1, "malloc(%d), %s:%d", BUFSIZE, __FILE__, __LINE__);

	if ((fd = open(outputFile, O_RDWR | O_CREAT | O_TRUNC, 0640)) ==
	    -1)
		err(1, "open(%s)", outputFile);

	for (;;) {
		if ((n = read(msgsock, buf, BUFSIZE)) < 0)
			err(1, "read(), %s:%d", __FILE__, __LINE__);
		t += n;
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
	struct sockaddr_in inetaddr;
	struct hostent *hostent;
	struct stat statb;
	off_t off = 0;
	size_t size;
	int i, fd, on, r, tcpsock;
	char *cp;

	on = 1;
	for (i = 1; i < 5; i++) {
		if ((tcpsock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
			err(1, "socket(), %s:%d", __FILE__, __LINE__);

		if (setsockopt(tcpsock,
		    SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
			err(1, "setsockopt(), %s:%d", __FILE__, __LINE__);

		size = getpagesize() -4;
		if (setsockopt(tcpsock,
		    SOL_SOCKET, SO_SNDBUF, (void *)&size, sizeof(size)) < 0)
			err(1, "setsockopt(SO_SNDBUF), %s:%d", __FILE__,
			    __LINE__);

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

	if ((fd = open(inputFile, O_RDWR)) == -1)
		err(1, "open(%s)", inputFile);

	if ((cp = mmap(NULL, statb.st_size, PROT_READ | PROT_WRITE,
	    MAP_SHARED, fd, 0)) == MAP_FAILED)
		err(1, "mmap");
	cp[0] = 1;

	if (sendfile(fd, tcpsock, 0, statb.st_size, NULL, &off, SF_NOCACHE)
	    == -1)
		err(1, "sendfile");

	return;
}

int
main(int argc, char **argv)
{
	pid_t pid;
	int e, s;

	if (argc != 4) {
		fprintf(stderr,
		    "Usage: %s <inputFile outputFile portNumber\n", argv[0]);
			return (1);
	}
	e = 0;
	inputFile = argv[1];
	outputFile = argv[2];
	port = atoi(argv[3]);

	if ((pid = fork()) == 0) {
		writer();
		exit(EXIT_SUCCESS);
	} else if (pid > 0) {
		reader();
		kill(pid, SIGINT);
		waitpid(pid, &s, 0);
		if (s != 0)
			e = 1;
	} else
		err(1, "fork(), %s:%d",  __FILE__, __LINE__);

	return (e);
}

#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2023 Peter Holm <pho@FreeBSD.org>
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

# D38549: msdosfs deextend: validate pages of the partial buffer

# https://people.freebsd.org/~pho/stress/log/log0420.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

set -eu
prog=$(basename "$0" .sh)
mkdir -p $mntpoint
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
newfs_msdos /dev/md$mdstart > /dev/null 2>&1
mount -t msdosfs /dev/md$mdstart $mntpoint
mount | grep $mntpoint
set +e

odir=`pwd`

cd /tmp
sed '1,/^EOF/d' < $odir/$0 > $prog.c
cc -o $prog -Wall -O0 $prog.c -pthread || exit 1
rm -f $prog.c
cd $mntpoint

in=inputFile
out=outputFile

/tmp/$prog $in $out 12345
ls -al

cd $odir
umount $mntpoint
mdconfig -d -u $mdstart
rm -f /tmp/$prog
exit
EOF
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <pthread_np.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int fd, port;
volatile int go;
char *inputFile;
char *outputFile;

#define FSIZE 936374
char buf[FSIZE];

static void
reader(void) {
	struct sockaddr_in inetaddr, inetpeer;
	socklen_t len;
	int on, n, tcpsock, msgsock;

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

	if ((fd = open(outputFile, O_RDWR | O_CREAT | O_TRUNC, 0640)) == -1)
		err(1, "open(%s)", outputFile);

	usleep(arc4random() % 1000);
	for (;;) {
		if ((n = read(msgsock, buf, FSIZE)) < 0)
			err(1, "read(), %s:%d", __FILE__, __LINE__);
		if (n == 0)
			break;

	}
	close(msgsock);
	close(tcpsock);
	close(fd);
	return;
}

static void *
thr(void *data __unused)
{
	pthread_set_name_np(pthread_self(), __func__);
	go = 1;
	while (go == 1) {
		ftruncate(fd, FSIZE / 2);
		ftruncate(fd, FSIZE);
	}

	return (NULL);
}

static void
writer(void) {
	struct hostent *hostent;
	struct sockaddr_in inetaddr;
	pthread_t tid;
	off_t off = 0;
	size_t size;
	int i, on, r, tcpsock;

	on = 1;
	for (i = 1; i < 5; i++) {
		if ((tcpsock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
			err(1, "socket(), %s:%d", __FILE__, __LINE__);

		if (setsockopt(tcpsock,
		    SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
			err(1, "setsockopt(), %s:%d", __FILE__, __LINE__);

		size = getpagesize();
		if (setsockopt(tcpsock, SOL_SOCKET, SO_SNDBUF, (void *)&size,
		    sizeof(size)) < 0)
			err(1, "setsockopt(SO_SNDBUF), %s:%d",
			    __FILE__, __LINE__);

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

	if ((fd = open(inputFile, O_RDWR | O_CREAT | O_TRUNC, 0640)) == -1)
		err(1, "open(%s)", inputFile);

	if (write(fd, buf, sizeof(buf)) != sizeof(buf))
		err(1, "write()");

	r = pthread_create(&tid, NULL, thr, NULL);
	if (r)
		errc(1, r, "pthread_create");

	usleep(5000);
	if (sendfile(fd, tcpsock, 0, FSIZE, NULL, &off, 0) == -1)
		err(1, "sendfile");

	usleep(arc4random() % 20000);
	go = 0;
	if ((r = pthread_join(tid, NULL)) != 0)
		errc(1, r, "pthread_join");

	_exit(0);
}

int
main(int argc, char **argv)
{
	pid_t pid;
	time_t start;

	if (argc != 4) {
		fprintf(stderr, "Usage: %s <inputFile outputFile portNumber\n",
		    argv[0]);
		return (1);
	}
	inputFile = argv[1];
	outputFile = argv[2];
	port = atoi(argv[3]);
	pthread_set_name_np(pthread_self(), __func__);

	start = time(NULL);
	while (time(NULL) - start < 120) {
		if ((pid = fork()) == 0) {
			writer();
		} else if (pid > 0) {
			reader();
			kill(pid, SIGINT);
			if (waitpid(pid, NULL, 0) != pid)
				err(1, "waitpid()");
		} else
			err(1, "fork(), %s:%d",  __FILE__, __LINE__);
	}

	return (0);
}

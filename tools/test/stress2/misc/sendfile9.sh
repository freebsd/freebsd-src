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

# "kmem are D+    0  0:00,10 /tmp/sendfile9 in out 76543" seen.
# Fixed in r315910

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/sendfile9.c
mycc -o sendfile9 -Wall -Wextra -O0 -g sendfile9.c || exit 1
rm -f sendfile9.c
cd $odir

mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart || exit 1
bsdlabel -w md$mdstart auto
newfs $newfs_flags -n md${mdstart}$part > /dev/null
mount /dev/md${mdstart}$part $mntpoint

cd $mntpoint
dd if=/dev/random of=in bs=1m count=50 status=none
/tmp/sendfile9 in out 76543
s=$?
cd $odir

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
done
[ $i -eq 6 ] && exit 1
mdconfig -d -u $mdstart
rm -rf /tmp/sendfile9
exit $s

EOF
#include <sys/param.h>
#include <sys/mman.h>
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

static int port;
static char *input, *output;

#define BUFSIZE 4096
#define PARALLEL 1
#define RUNTIME (1 * 60)

static void
mess(void)
{
	off_t length;
	int fd;

	if ((fd = open(input, O_RDWR)) == -1)
		err(1, "open(%s)", input);
	length = arc4random() % 100 * 1024 * 1024;
	if (ftruncate(fd, length) == -1)
		err(1, "truncate(%jd)", length);
	close(fd);
}

static void
reader(void) {
	struct sockaddr_in inetaddr, inetpeer;
	socklen_t len;
	int tcpsock, msgsock;
	int on;
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

	if ((buf = malloc(BUFSIZE)) == NULL)
		err(1, "malloc(%d), %s:%d", BUFSIZE, __FILE__, __LINE__);

	if ((fd = open(output, O_RDWR | O_CREAT | O_TRUNC, 0640)) == -1)
		err(1, "open(%s)", output);

	for (;;) {
		if ((n = read(msgsock, buf, BUFSIZE)) < 0)
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
	struct sockaddr_in inetaddr;
	struct hostent *hostent;
	struct stat statb;
	off_t off = 0;
	size_t size;
	int i, r, fd;
	int tcpsock, on;

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

        if (stat(input, &statb) != 0)
                err(1, "stat(%s)", input);

	if ((fd = open(input, O_RDONLY)) == -1)
		err(1, "open(%s)", input);

	if (sendfile(fd, tcpsock, 0, statb.st_size, NULL, &off, 0) == -1)
		err(1, "sendfile");

	return;
}

static void
test(void)
{
	pid_t pid;

	if ((pid = fork()) == 0) {
		writer();
		_exit(0);
	}
	reader();
	kill(pid, SIGINT);
	if (waitpid(pid, NULL, 0) != pid)
		err(1, "waitpid(%d)", pid);

	_exit(0);
}

int
main(int argc, char *argv[])
{
	time_t start;
	int e, i, j, pids[PARALLEL], status;

	if (argc != 4) {
		fprintf(stderr, "Usage: %s <input file> <output file>\n",
		    argv[0]);
		exit(1);
	}
	input = argv[1];
	output = argv[2];
	port = atoi(argv[3]);
	e = 0;

	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME && e == 0) {
		for (i = 0; i < PARALLEL; i++) {
			if ((pids[i] = fork()) == 0)
				test();
		}
		for (j = 0; j < 100; j++) {
			mess();
			usleep(arc4random() % 10000);
		}
		for (i = 0; i < PARALLEL; i++) {
			if (waitpid(pids[i], &status, 0) == -1)
				err(1, "waitpid(%d)", pids[i]);
			e += status == 0 ? 0 : 1;
		}
	}

	return (e);
}

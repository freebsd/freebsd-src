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

# Copy of sendfile8.sh with size validation added.

# No problems seen (after r315910).

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/sendfile10.c
mycc -o sendfile10 -Wall -Wextra -O0 -g sendfile10.c || exit 1
rm -f sendfile10.c
cd $odir

mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart || exit 1
newfs $newfs_flags -n md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

cd $mntpoint
dd if=/dev/random of=template bs=1m count=50 status=none
/tmp/sendfile10 template in out 76543
s=$?
cd $odir

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
done
[ $i -eq 6 ] && exit 1
mdconfig -d -u $mdstart
rm -rf /tmp/sendfile10
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

static volatile off_t *share;
static int port;
static char *input, *output;

#define BUFSIZE 4096
#define MX (100 * 1024 * 1024)
#define OSZ 1
#define PARALLEL 1
#define RUNTIME (2 * 60)
#define SZ 0

static void
mess(void)
{
	off_t length;
	int fd;

	if ((fd = open(input, O_RDWR)) == -1)
		err(1, "open(%s)", input);
	length = arc4random() % MX;
	if (ftruncate(fd, length) == -1)
		err(1, "truncate(%jd)", length);
	share[SZ] = length;
	close(fd);
}

static void
reader(void) {
	off_t t __unused;
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
	alarm(10);
	if ((msgsock = accept(tcpsock,
	    (struct sockaddr *)&inetpeer, &len)) < 0)
		err(1, "accept(), %s:%d", __FILE__, __LINE__);
	alarm(0);

	t = 0;
	if ((buf = malloc(BUFSIZE)) == NULL)
		err(1, "malloc(%d), %s:%d", BUFSIZE, __FILE__, __LINE__);

	if ((fd = open(output, O_RDWR | O_CREAT | O_TRUNC, 0640)) == -1)
		err(1, "open(%s)", output);

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
#if 0
	if (t != share[SZ] && t != share[OSZ]) {
		fprintf(stderr, "1) Send size %lu, original size %lu, "
		    "receive size %lu\n",
		    (unsigned long)share[SZ],
		    (unsigned long)share[OSZ],
		    (unsigned long)t);
		exit(1);
	}
#endif
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

	if ((fd = open(input, O_RDONLY)) == -1)
		err(1, "open(%s)", input);

        if (fstat(fd, &statb) != 0)
                err(1, "stat(%s)", input);
	share[SZ] = statb.st_size;
	if (sendfile(fd, tcpsock, 0, statb.st_size, NULL, &off, 0) == -1)
		err(1, "sendfile");
	close(fd);
	close(tcpsock);

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
	struct stat statin, statorig, statout;
	size_t len;
	time_t start;
	int e, i, pids[PARALLEL], status;
	char help[80], *template;

	if (argc != 5) {
		fprintf(stderr, "Usage: %s <template> <input file> "
		    "<output file> <port>\n", argv[0]);
		exit(1);
	}
	template = argv[1];
	input = argv[2];
	output = argv[3];
	port = atoi(argv[4]);
	snprintf(help, sizeof(help), "cp %s %s", template, input);
	e = 0;
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME && e == 0) {
		system(help);
		if (stat(input, &statin) == -1)
			err(1, "stat(%s)", input);
		share[OSZ] = statin.st_size;
		for (i = 0; i < PARALLEL; i++) {
			if ((pids[i] = fork()) == 0)
				test();
		}
		usleep(arc4random() % 10000);
		mess();
		for (i = 0; i < PARALLEL; i++) {
			if (waitpid(pids[i], &status, 0) == -1)
				err(1, "waitpid(%d)", pids[i]);
			e += status == 0 ? 0 : 1;
		}
		if (stat(template, &statorig) == -1)
			err(1, "stat(%s)", input);
		if (stat(input, &statin) == -1)
			err(1, "stat(%s)", input);
		if (stat(output, &statout) == -1)
			err(1, "stat(%s)", output);
		if (statout.st_size >= MX) {
			fprintf(stderr, "Send size %lu, original size %lu, "
			    "receive size %lu\n",
			    (unsigned long)statin.st_size,
			    (unsigned long)statorig.st_size,
			    (unsigned long)statout.st_size);
			system("ls -l | grep -v total");
			exit(1);
		}
	}

	return (e);
}

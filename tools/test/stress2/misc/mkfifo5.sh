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

# mkfifo(2), select(2) with tmpfs(5) scenario.

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/mkfifo5.c
mycc -o mkfifo5 -Wall -Wextra -O0 -g mkfifo5.c || exit 1
rm -f mkfifo5.c
cd $odir

mount | grep -q "on $mntpoint " && umount -f $mntpoint
mount -o size=1g -t tmpfs tmpfs $mntpoint

fifo=$mntpoint/fifo.file
cd $mntpoint
/tmp/mkfifo5 $fifo
s=$?
cd $odir

while mount | grep "on $mntpoint " | grep -q tmpfs; do
	umount $mntpoint || sleep 1
done
rm -rf /tmp/mkfifo5
exit $s

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static char *path;

#define PARALLEL 1

static void
reader(void)
{
	fd_set rset;
	struct timeval timeout;
	int fd, n, r;
	char ch;

	do {
		if((fd = open(path, O_RDONLY)) == -1)
			if (errno != EINTR)
				err(1, "open(%s, O_RDONLY)",
				    path);
		if (fd == -1)
			warn("open(%s) ro", path);
	} while (fd == -1);

	/* Read one character */
	FD_ZERO(&rset);
	FD_SET(fd, &rset);
	if ((n = select(fd + 1, &rset, NULL, NULL, NULL)) < 0)
		if (errno != EINTR)
			err(1, "select()");
	if (n == 1 && FD_ISSET(fd, &rset)) {
		r = read(fd, &ch, 1);
		if (r == -1)
			err(1, "read");
		if (r == 0)
			fprintf(stderr, "read(1): EOF\n");
	}

	/* timeout */
	ch = 'z';
	FD_ZERO(&rset);
	FD_SET(fd, &rset);
	timeout.tv_sec  = 1;
	timeout.tv_usec = 0;
	if ((n = select(fd + 1, &rset, NULL, NULL, &timeout)) < 0)
		if (errno != EINTR)
			err(1, "select()");
	if (n != 1)
		fprintf(stderr, "FAIL Expected n == 0, got %d\n", n);
	if (n == 1 && FD_ISSET(fd, &rset)) {
		r = read(fd, &ch, 1);
		if (r == -1)
			err(1, "read");
		if (r == 0)
			fprintf(stderr, "read(2): EOF\n");
	}

	/* timeout */
	ch = 'z';
	FD_ZERO(&rset);
	FD_SET(fd, &rset);
	timeout.tv_sec  = 1;
	timeout.tv_usec = 0;
	if ((n = select(fd + 1, &rset, NULL, NULL, &timeout)) < 0)
		if (errno != EINTR)
			err(1, "select()");
	if (n != 1)
		fprintf(stderr, "FAIL Expected n == 0, got %d\n", n);
	if (n == 1 && FD_ISSET(fd, &rset)) {
		r = read(fd, &ch, 1);
		if (r == -1)
			err(1, "read");
		if (r != 0)
			fprintf(stderr, "read(3): %c\n", ch);
	}

	if (close(fd) == -1)
		err(1, "close() in child");
	_exit(n == 1 ? 0 : 1);
}

static void
writer(void)
{
	int fd;

	do {
		if ((fd = open(path, O_WRONLY)) == -1)
			if (errno != EINTR)
				err(1, "open(%s, O_WRONLY)",
				    path);
		if (fd == -1)
			warn("open(%s) wr", path);
	} while (fd == -1);
	if (write(fd, "a", 1) != 1)
		err(1, "write one");
	if (write(fd, "b", 1) != 1)
		err(1, "write one");
	if (close(fd) == -1)
		warn("close() in parent");
}

static void
test(void)
{
	pid_t pid;
	int status;

	if ((pid = fork()) == 0)
		reader();
	writer();

	if (waitpid(pid, &status, 0) != pid)
		err(1, "waitpid(%d)", pid);

	_exit(status != 0);
}

int
main(int argc __unused, char *argv[])
{
	int e, i, pids[PARALLEL], status;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <fifo file>\n", argv[0]);
		exit(1);
	}
	path = argv[1];
	e = 0;

	unlink(path);
	if (mkfifo(path, 0640) == -1)
		err(1, "mkfifo(%s)", path);

	for (i = 0; i < PARALLEL; i++) {
		if ((pids[i] = fork()) == 0)
			test();
	}
	for (i = 0; i < PARALLEL; i++) {
		if (waitpid(pids[i], &status, 0) == -1)
			err(1, "waitpid(%d)", pids[i]);
		e += status == 0 ? 0 : 1;
	}

	return (e);
}

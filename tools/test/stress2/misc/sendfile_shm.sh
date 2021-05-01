#!/bin/sh

#
# Copyright (c) 2013 EMC Corp.
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

# Sendfile(2) over posix shmfd
# Test scenario by kib@

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

[ -r /boot/kernel/kernel ] || exit 0
here=`pwd`
dir=`dirname $diskimage`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > sendfile_shm.c
mycc -o sendfile_shm -Wall -Wextra -O2 sendfile_shm.c || exit 1
rm -f sendfile_shm.c
cd $here

daemon ../testcases/swap/swap -t 2m -i 20 > /dev/null 2>&1
sleep 5
for i in `jot 10`; do
	/tmp/sendfile_shm /boot/kernel/kernel $dir/sendfile_shm.$i > \
	    /dev/null &
done
wait
while pkill -9 swap; do
	sleep .5
done
for i in `jot 10`; do
	cmp -s /boot/kernel/kernel $dir/sendfile_shm.$i 2>/dev/null ||
	    { e=1; ls -al $dir/sendfile_shm.* /boot/kernel/kernel; }
	rm -f $dir/sendfile_shm.$i
	[ $e ] && break
done
[ -n "$e" ] && echo FAIL
wait

rm -f /tmp/sendfile_shm
exit
EOF
/* $Id: sendfile_shm1.c,v 1.2 2013/08/25 14:35:14 kostik Exp kostik $ */

#include <sys/types.h>
#include <sys/event.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <err.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	PAGE_SIZE 4096

static void
load(int infd, int shmfd, off_t size)
{
	off_t c, r;
	char buf[100];

	for (c = 0; c < size; c += r) {
		r = read(infd, buf, sizeof(buf));
		if (r == -1)
			err(1, "read disk");
		else if (r == 0)
			errx(1, "too short");
		write(shmfd, buf, r);
	}
}

static void
sendfd(int s, int shmfd, off_t size)
{
	off_t sbytes;
	int error;

	error = sendfile(shmfd, s, 0, size, NULL, &sbytes, 0);
	if (error == -1)
		err(1, "sendfile(%d, %d, %jd)", s, shmfd, size);
	printf("sent %jd bytes, requested %jd\n", (uintmax_t)sbytes,
	    (uintmax_t)size);
}

static void
receivefd(int s, int outfd, off_t size)
{
	char buf[100];
	off_t c, r, r1;

	for (c = 0; c < size; c += r) {
		r = read(s, buf, sizeof(buf));
		if (r == -1)
			err(1, "read sock");
		else if (r == 0)
			break;
		else {
			r1 = write(outfd, buf, r);
			if (r1 == -1)
				err(1, "write disk");
			else if (r1 != r) {
				err(1, "short write %jd %jd",
				    (uintmax_t)r, (uintmax_t)r1);
			}
		}
	}
	printf("received %jd bytes\n", (uintmax_t)c);
}

int
main(int argc, char *argv[])
{
	struct stat st;
	struct sigaction sa;
	pid_t child;
	int s[2], infd, outfd, shmfd, error;

	if (argc != 3) {
		fprintf(stderr, "usage: sendfile_shm infile outfile\n");
		exit(2);
	}

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	error = sigaction(SIGPIPE, &sa, NULL);
	if (error == -1)
		err(1, "sigaction SIGPIPE");

	infd = open(argv[1], O_RDONLY);
	if (infd == -1)
		err(1, "open %s", argv[1]);
	error = fstat(infd, &st);
	if (error == -1)
		err(1, "stat");

	outfd = open(argv[2], O_RDWR | O_CREAT | O_TRUNC, 0666);
	if (outfd == -1)
		err(1, "open %s", argv[2]);

	shmfd = shm_open(SHM_ANON, O_CREAT | O_RDWR, 0600);
	if (shmfd == -1)
		err(1, "shm_open");
	error = ftruncate(shmfd, st.st_size);
	if (error == -1)
		err(1, "ftruncate");
	load(infd, shmfd, st.st_size);

	error = socketpair(AF_UNIX, SOCK_STREAM, 0, s);
	if (error == -1)
		err(1, "socketpair");

	fflush(stdout);
	fflush(stderr);
	child = fork();
	if (child == -1)
		err(1, "fork");
	else if (child == 0) {
		close(s[1]);
		sendfd(s[0], shmfd, st.st_size);
		exit(0);
	} else {
		close(shmfd);
		close(s[0]);
		sleep(1);
		receivefd(s[1], outfd, st.st_size);
	}

	return (0);
}

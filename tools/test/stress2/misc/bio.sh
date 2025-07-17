#!/bin/sh

#
# Copyright (c) 2015 EMC Corp.
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

# mmap() & read()/write() on same file.
# Test scenario suggestion by: jeff@

# Out of VM deadlock seen:
# https://people.freebsd.org/~pho/stress/log/jeff114.txt

# panic: deadlkres: possible deadlock detected:
# https://people.freebsd.org/~pho/stress/log/jeff115.txt

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
ps=`sysctl -n hw.pagesize`
sed "1,/^EOF/d;s/\$ps/$ps/" < $odir/$0 > $dir/bio.c
mycc -o bio -Wall -Wextra -O0 -g bio.c || exit 1
rm -f bio.c
cd $odir

mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart || exit 1
newfs -n md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

(cd $mntpoint; /tmp/bio) &
pid1=$!
sleep 5
(cd ../testcases/swap; ./swap -t 5m -i 20 -k -l 100 -h) &
pid2=$!

while pgrep -q bio; do
	sleep 2
done

while pgrep -q swap; do
	pkill -9 swap
done
wait $pid2
wait $pid1
s=$?

while mount | grep "on $mntpoint " | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -rf /tmp/bio
exit $s

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <machine/atomic.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define PS $ps	/* From hw.pagesize */
#define SYN1 0
#define SYN2 1
#define INDX 2

#define PAGES (512 * 1024 * 1024 / PS)	/* 512MB file size */
#define PARALLEL 3
#define RUNTIME (5 * 60)
#define TIMEOUT (30 * 60)

char buf[PS];

void
test(int inx)
{
	pid_t pid;
	size_t i, len, slen;
	time_t start;
	volatile u_int *share;
	int fd, r;
	u_int *ip, val;
	char file[80];

	slen = PS;
	if ((share = mmap(NULL, slen, PROT_READ | PROT_WRITE, MAP_ANON |
	    MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	snprintf(file, sizeof(file), "file.%06d", inx);
	if ((fd = open(file, O_RDWR | O_CREAT | O_TRUNC, 0640)) < 0)
		err(1, "%s", file);

	for (i = 0; i < PAGES; i++) {
		if (write(fd, buf, sizeof(buf)) != sizeof(buf))
			err(1, "write error");
	}

	len = PS * PAGES * sizeof(u_int);
	if ((ip = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED,
	    fd, 0)) == MAP_FAILED)
			err(1, "mmap");

	start = time(NULL);
	if ((pid = fork()) == 0) {
		alarm(2 * RUNTIME);
		/* mmap read / write access */
		for (i = 0; i < (size_t)(PAGES * PS); i += PS) {
			while (share[SYN1] == 0)
				sched_yield();
			atomic_add_int(&share[SYN1], -1);
			if (ip[share[INDX] / sizeof(u_int)] != share[INDX])
				warn("child expected %d, but got %d\n",
				    ip[share[INDX] / sizeof(u_int)],
				    share[INDX]);
			share[INDX] += PS;
			ip[share[INDX] / sizeof(u_int)] = share[INDX];
			atomic_add_int(&share[SYN2], 1); /* signal parent */
			if (i % 1000 == 0 && time(NULL) - start > TIMEOUT)
				errx(1, "Timed out");;
		}
		_exit(0);
	}
	if (pid == -1)
		err(1, "fork()");
	share[INDX] = 0;
	atomic_add_int(&share[SYN2], 1);
	alarm(2 * RUNTIME);
	for (i = 0; i < (size_t)(PAGES * PS); i += PS) {
		while (share[SYN2] == 0)
			sched_yield();
		atomic_add_int(&share[SYN2], -1);
		if (lseek(fd, share[INDX], SEEK_SET) == -1)
			err(1, "lseek error");
		if ((r = read(fd, &val, sizeof(val))) != sizeof(val))
			err(1, "parent read read %d bytes", r);
		if (val != share[INDX])
			warn("parent expected %d, but got %d\n",
			    share[INDX], val);
		val += PS;
		if (lseek(fd, val, SEEK_SET) == -1)
			err(1, "lseek error");
		if (write(fd, &val, sizeof(val)) != sizeof(val))
			err(1, "write");

		atomic_add_int(&share[SYN1], 1); /* signal child */
		if (i % 1000 == 0 && time(NULL) - start > TIMEOUT)
			errx(1, "Timed out");;
	}
	atomic_add_int(&share[SYN2], -1);
	if (waitpid(pid, NULL, 0) != pid)
		err(1, "wait");

	if (munmap(ip, len) == -1)
		err(1, "unmap()");
	if (munmap((void *)share, slen) == -1)
		err(1, "unmap()");
	close(fd);
	if (unlink(file) == -1)
		err(1, "unlink(%s)", file);

	_exit(0);
}

int
main(void)
{
	pid_t pids[PARALLEL];
	time_t start;
	int i, s, status;

	start = time(NULL);
	s = 0;
	while ((time(NULL) - start) < RUNTIME && s == 0) {
		for (i = 0; i < PARALLEL; i++) {
			if ((pids[i] = fork()) == 0)
				test(i);
		}
		for (i = 0; i < PARALLEL; i++) {
			if (waitpid(pids[i], &status, 0) == -1)
				err(1, "waitpid(%d)", pids[i]);
			if (status != 0)
				fprintf(stderr, "Child exit status = %d\n",
					status);
			s += status == 0 ? 0 : 1;
		}
	}

	return (s);
}

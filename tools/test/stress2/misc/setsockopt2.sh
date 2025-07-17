#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Dell EMC Isilon
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

# "panic: Assertion in_epoch(net_epoch_preempt) failed at raw_ip6.c:742"
# https://people.freebsd.org/~pho/stress/log/setsockopt2.txt

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/setsockopt2.c
mycc -o setsockopt2 -Wall -Wextra -O0 -g setsockopt2.c || exit 1
rm -f setsockopt2.c
cd $odir

set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

(cd ../testcases/swap; ./swap -t 5m -i 20) &
cd $mntpoint
$dir/setsockopt2
s=$?
[ -f setsockopt2.core -a $s -eq 0 ] &&
    { ls -l setsockopt2.core; mv setsockopt2.core $dir; s=1; }
cd $odir
while pkill swap; do :; done
wait

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 6 ] &&
	    { echo FATAL; fstat -mf $mntpoint; exit 1; }
done
mdconfig -d -u $mdstart
rm -rf $dir/setsockopt2
exit $s

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int debug; /* set to 1 for debug output */
static volatile u_int *share;

#define PARALLEL 128
#define RUNTIME (2 * 60)
#define SYNC 0

static void
test(void)
{
	struct sockaddr_un sun;
	pid_t pid;
	time_t start;
	char file[80];
	int domain, fd, i, one, prot, success, typ;

	success = 0;
	snprintf(file, sizeof(file), "setsockopt2.socket.%d", getpid());
	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_LOCAL;
	sun.sun_len = sizeof(sun);
	snprintf(sun.sun_path, sizeof(sun.sun_path), "%s", file);
	unlink(file);

	start = time(NULL);
	while (time(NULL) - start < 30) {
		share[SYNC] = 0;
		fd = -1;
		while (fd == -1) {
			domain = arc4random() % 256;
			typ = arc4random() % 10;
			if (arc4random() % 100 < 20)
				prot = arc4random() % 10;
			else
				prot = 0;
			if ((fd = socket(domain, typ, prot)) == -1)
				continue;
			close(fd);
		}
		pid = fork();
		if (pid < 0)
			err(1, "fork");
		if (pid == 0) {
			// listen
			fd = socket(domain, typ, prot);
			if (fd < 0)
				_exit(0);
			one = 1;
			if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one,
			    sizeof(one)) < 0)
				err(1, "setsockopt");
			if (bind(fd, (struct sockaddr *)&sun, sizeof(sun)) < 0)
				_exit(0);
			if (listen(fd, 10) == 0) {
				share[SYNC] = 1;
				usleep(random() % 10000);
			}
			(void)close(fd);
			(void)unlink(file);
			_exit(0);
		}
		fd = socket(domain, typ, prot);
		if (fd == -1)
			goto bad;

		setsockopt(fd, 0xffff, 0x80, 0x0, 0x0);
		sun.sun_len = arc4random() % 128;
		sun.sun_family = arc4random() % 10;
		setsockopt(fd, 0x6, 0x401, &sun, 0x14);

		for (i = 0; share[SYNC] == 0 && i < 10; i++)
			usleep(100);
		if (connect(fd, (struct sockaddr *)&sun, sizeof(sun)) != -1)
			success++;
		usleep(random() % 100);
bad:
		(void)close(fd);
		if (waitpid(pid, NULL, 0) != pid)
			err(1, "waitpid(%d)", pid);
	}
	if (debug != 0 && success == 0)
		fprintf(stderr, "No calls to connect() succeded.\n");

	_exit(0);
}

int
main(void)
{
	pid_t pids[PARALLEL];
	size_t len;
	time_t start;
	int e, i, status;

	e = 0;
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME) {
		for (i = 0; i < PARALLEL; i++) {
			if ((pids[i] = fork()) == 0)
				test();
			if (pids[i] == -1)
				err(1, "fork()");
		}
		for (i = 0; i < PARALLEL; i++) {
			if (waitpid(pids[i], &status, 0) == -1)
				err(1, "waitpid(%d)", pids[i]);
			if (status != 0) {
				if (WIFSIGNALED(status))
					fprintf(stderr,
					    "pid %d exit signal %d\n",
					    pids[i], WTERMSIG(status));
			}
			e += status == 0 ? 0 : 1;
		}
	}

	return (e);
}

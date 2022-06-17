#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2020 Peter Holm <pho@FreeBSD.org>
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

# A socketpair() test scenario illustrating issues with a missing EOF
# Fixed by r361037 + r361056

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/socketpair4.c
mycc -o socketpair4 -Wall -Wextra -O0 -g socketpair4.c || exit 1
rm -f socketpair4.c
cd $odir

set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
newfs_flags=""
newfs $newfs_flags -n md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

cd $mntpoint
dd if=/dev/random of=input bs=4k count=100 status=none

n=1
start=` date +%s`
while [ $((` date +%s` - start)) -lt 180 ]; do
	rm -f output
	umount $mntpoint 2>/dev/null # busy umount
	$dir/socketpair4
	s=$?
	[ $s -ne 0 ] && { echo "Loop #$n"; ls -l; s=1; break; }
	cmp -s input output || break
	[ `stat -f '%z' input` -ne ` stat -f '%z' output` ] && break
	n=$((n + 1))
done
while pgrep -q swap; do
	pkill swap
done
cmp -s input output || { echo "Loop #$n"; ls -l; s=1; }
[ -f socketpair4.core -a $s -eq 0 ] &&
    { ls -l socketpair4.core; mv socketpair4.core $dir; s=1; }
cd $odir

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 6 ] &&
	    { echo FATAL; fstat -mf $mntpoint; exit 1; }
done
mdconfig -d -u $mdstart
rm -rf $dir/socketpair4
exit $s

EOF
#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void
test(void)
{
	struct stat st;
	off_t rd, written;
	pid_t pid;
	int error, from, n, status, sv[2], to;
	char buf[4086];
	const char *from_name, *to_name;

	from_name = "input";
	to_name = "output";

	if ((error = socketpair(AF_UNIX, SOCK_STREAM, 0, sv)) == -1)
		err(1, "socketpair");

	if ((from = open(from_name, O_RDONLY)) == -1)
		err(1, "open read %s", from_name);

	if ((error = fstat(from, &st)) == -1)
		err(1, "stat %s", from_name);

	pid = fork();
	if (pid == -1)
		err(1, "fork");
	else if (pid != 0) {
		setproctitle("parent");
		close(sv[1]);

		written = 0;
		for (;;) {
			n = read(from, buf, sizeof(buf));
			if (n == -1)
				err(1, "read(from)");
			else if (n == 0)
				break;
			written += n;
			if (write(sv[0], buf, n) != n)
				err(1, "write()");
			if (error == -1)
				err(1, "write");
		}
		if (written != st.st_size)
			fprintf(stderr, "%d written, expected %d\n",
			    (int)written, (int)st.st_size);
		close(from);
		close(sv[0]);
		if (waitpid(pid, &status, 0) != pid)
			err(1, "waitpid(%d)", pid);
		_exit(WEXITSTATUS(status));
	} else {
		setproctitle("child");
		sleep(1);
		close(from);
		close(sv[0]);

		if ((to = open(to_name, O_RDWR | O_CREAT, DEFFILEMODE)) == -1)
			err(1, "open write %s", to_name);

		rd = 0;
		for (;;) {
			n = read(sv[1], buf, sizeof(buf));
			if (n == -1) {
				if (rd == st.st_size) {
					err(1, "Expected EOF on read(socket)");
					break;
				}
				err(1, "read(socket), n=%d, rd=%d", n,
				    (int)rd);
			}
			if (n == 0)
				break;
			rd += n;
			if (write(to, buf, n) != n)
				err(1, "write()");
		}
		close(to);
		close(sv[1]);
		if (rd != st.st_size)
			fprintf(stderr, "Short read %d, expected %d\n",
			    (int)rd, (int)st.st_size);
		_exit(0);
	}
}

int
main(void)
{
	pid_t pid;
	int status;

	if ((pid = fork()) == 0)
		test();
	if (pid == -1)
		err(1, "fork()");
	if (waitpid(pid, &status, 0) == -1)
		err(1, "waitpid(%d)", pid);
	if (status != 0) {
		if (WIFSIGNALED(status))
			fprintf(stderr,
			    "pid %d exit signal %d\n",
			    pid, WTERMSIG(status));
	}

	return (WEXITSTATUS(status));
}

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

# Variation of sendfile21.sh: without the use of socketpair().
# No problems seen.

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/sendfile22.c
mycc -o sendfile22 -Wall -Wextra -O0 -g sendfile22.c || exit 1
rm -f sendfile22.c
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

(cd $odir/../testcases/swap; ./swap -t 5m -i 20 -l 100 > /dev/null) &
sleep 30
n=1
start=` date +%s`
while [ $((` date +%s` - start)) -lt 180 ]; do
	rm -f output
	umount $mntpoint 2>/dev/null # busy umount
	$dir/sendfile22
	s=$?
	[ $s -ne 0 ] &&
	    pkill sendfile22
	cmp -s input output || break
	[ `stat -f '%z' input` -ne ` stat -f '%z' output` ] && break
	n=$((n + 1))
done
while pgrep -q swap; do
	pkill swap
done
cmp -s input output || { echo "Loop #$n"; ls -l; s=1; }
wait
[ -f sendfile22.core -a $s -eq 0 ] &&
    { ls -l sendfile22.core; mv sendfile22.core $dir; s=1; }
cd $odir

for i in `jot 18`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 18 ] &&
	    { echo FATAL; fstat -mf $mntpoint; exit 1; }
done
mdconfig -d -u $mdstart
rm -rf $dir/sendfile22
exit $s

EOF
#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

static void
test(void)
{
	struct stat st;
	struct sockaddr_in inetaddr, inetpeer;
	struct hostent *hostent;
	socklen_t len;
	off_t i, j, rd, written, pos;
	pid_t pid;
	int error, from, i1, msgsock, n, on, port, r, status, tcpsock, to;
	char buf[4086], *cp;
	const char *from_name, *to_name;

	from_name = "input";
	to_name = "output";
	port = 12345;

	if ((from = open(from_name, O_RDONLY)) == -1)
		err(1, "open read %s", from_name);

	if ((error = fstat(from, &st)) == -1)
		err(1, "stat %s", from_name);

	pid = fork();
	if (pid == -1)
		err(1, "fork");
	else if (pid != 0) {
		setproctitle("parent");

		alarm(300);
		on = 1;
		for (i1 = 1; i1 < 5; i1++) {
			if ((tcpsock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
				err(1, "socket(), %s:%d", __FILE__, __LINE__);

			if (setsockopt(tcpsock,
			    SOL_SOCKET, SO_REUSEADDR, (char *)&on,
			    sizeof(on)) < 0)
				err(1, "setsockopt(), %s:%d", __FILE__,
						__LINE__);

			hostent = gethostbyname ("localhost");
			bzero((char *) &inetaddr, sizeof(inetaddr));
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

		if ((cp = mmap(NULL, st.st_size, PROT_READ,
		    MAP_PRIVATE, from, 0)) == MAP_FAILED)
			err(1, "mmap");
		if (fsync(from) == -1)
			err(1, "fsync()");

		for (i = 0, j = 0; i < st.st_size; i += PAGE_SIZE, j++) {
			(void)cp[i];
			if (j % 2 == 1) {
				if (msync(cp + i, PAGE_SIZE, MS_INVALIDATE)
				    == -1)
					err(1, "msync(), j = %d", (int)j);
			}
		}
		if (munmap(cp, st.st_size) == -1)
			err(1, "munmap()");

		pos = 0;
		for (;;) {
			error = sendfile(from, tcpsock, pos, st.st_size -
			    pos, NULL, &written, 0);
			if (error == -1)
				err(1, "sendfile");
			if (written != st.st_size)
				fprintf(stderr, "sendfile sent %d bytes\n",
				    (int)written);
			pos += written;
			if (pos == st.st_size)
				break;
		}
		if (pos != st.st_size)
			fprintf(stderr, "%d written, expected %d\n",
			    (int)pos, (int)st.st_size);
		close(tcpsock);
		if (waitpid(pid, &status, 0) != pid)
			err(1, "waitpid(%d)", pid);
	} else {
		setproctitle("child");
		close(from);
		alarm(300);
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

		if ((to = open(to_name, O_RDWR | O_CREAT, DEFFILEMODE)) ==
		    -1)
			err(1, "open write %s", to_name);

		rd = 0;
		for (;;) {
			n = read(msgsock, buf, sizeof(buf));
			if (n == -1)
				err(1, "read");
			else if (n == 0)
				break;
			rd += n;
			if (write(to, buf, n) != n)
				err(1, "write()");
		}
		close(to);
		if (rd != st.st_size)
			fprintf(stderr, "Short read %d, expected %d\n",
			    (int)rd, (int)st.st_size);
		_exit(0);
	}

	_exit(0);
}

int
main(void)
{
	pid_t pid;
	int e, status;

	e = 0;
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
	e += status == 0 ? 0 : 1;

	return (e);
}

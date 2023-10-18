#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Peter Holm <pho@FreeBSD.org>
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

# O_CREAT|O_EXCL|O_EXLOCK atomic implementation test.
# Lots of input from kib@

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/exlock2.c
mycc -o exlock2 -Wall -Wextra -O0 -g exlock2.c || exit 1
rm -f exlock2.c
cd $odir

$dir/exlock2
s=$?
[ -f exlock2.core -a $s -eq 0 ] &&
    { ls -l exlock2.core; mv exlock2.core $dir; s=1; }
cd $odir

rm -f $dir/exlock2 /tmp/exlock2.*.file
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
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static _Atomic(int) *share;
static int debug; /* Set to "1" for debug output */
static int quit;
static char file[80];

#define RUNTIME (2 * 60)
#define SYNC 0

static void
handler(int s __unused)
{
	quit = 1;
}

static void
test1(void)
{
	time_t start;
	int fd, n;

	signal(SIGHUP, handler);
	n = 0;
	start = time(NULL);
	while (time(NULL) - start < RUNTIME && quit == 0) {
		n++;
		if ((fd = open(file, O_RDWR|O_CREAT|O_EXCL|O_EXLOCK,
		    DEFFILEMODE)) == -1)
			err(1, "open(%s) creat", file);
		unlink(file);
		if (write(fd, "test", 5) != 5)
			err(1, "write()");
		while (share[SYNC] == 1)
			;	/* wait for test2 to signal "done" */
		close(fd);
	}
	if (debug != 0)
		fprintf(stderr, "%s: n = %d\n", __func__, n);

	_exit(0);
}

static void
test2(void)
{
	struct flock fl;
	struct stat st;
	time_t start;
	int e, fd;

	e = 0;
	fd = 0;
	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		share[SYNC] = 1;
		if ((fd = open(file, O_RDWR)) == -1)
			goto out;
		memset(&fl, 0, sizeof(fl));
		fl.l_start = 0;
		fl.l_len = 0;
		fl.l_type = F_WRLCK;
		fl.l_whence = SEEK_SET;
		if (fcntl(fd, F_SETLK, &fl) < 0) {
			if (errno != EAGAIN)
				err(1, "fcntl(F_SETFL)");
			goto out;
		}
		/* test1 must have dropped the lock */
		fprintf(stderr, "%s got the lock.\n", __func__);
		if (fstat(fd, &st) == -1)
			err(1, "stat(%s)", file);
		/* As test1 has opened the file exclusivly, this
		   should not happen */
		if (st.st_size == 0)
			fprintf(stderr, "%s has size 0\n", file);
		e = 1;
		break;
out:
		if (fd != -1)
			close(fd);
		share[SYNC] = 0;
		usleep(100);
	}
	if (debug != 0 && e != 0)
		system("ps -Uroot | grep -v grep | grep  /tmp/exlock2 | "\
		    "awk '{print $1}' | xargs procstat -f");
	share[SYNC] = 0;

	_exit(e);
}

int
main(void)
{
	pid_t pid1, pid2;
	size_t len;
	int e, status;

	e = 0;
	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	snprintf(file, sizeof(file), "/tmp/exlock2.%d.file", getpid());
	if ((pid1 = fork()) == 0)
		test1();
	if (pid1 == -1)
		err(1, "fork()");

	if ((pid2 = fork()) == 0)
		test2();
	if (pid2 == -1)
		err(1, "fork()");

	if (waitpid(pid2, &status, 0) != pid2)
		err(1, "waitpid(%d)", pid2);
	if (status != 0) {
		if (WIFSIGNALED(status))
			fprintf(stderr,
			    "pid %d exit signal %d\n",
			    pid2, WTERMSIG(status));
	}
	e += status == 0 ? 0 : 1;
	kill(pid1, SIGHUP);
	if (waitpid(pid1, &status, 0) != pid1)
		err(1, "waitpid(%d)", pid1);
	if (status != 0) {
		if (WIFSIGNALED(status))
			fprintf(stderr,
			    "pid %d exit signal %d\n",
			    pid1, WTERMSIG(status));
	}
	e += status == 0 ? 0 : 1;

	return (e);
}

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

. ../default.cfg

# "Assertion vap->va_type == VDIR failed" seen on non HEAD.

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/mkfifo4.c
mycc -o mkfifo4 -Wall -Wextra -O0 -g mkfifo4.c || exit 1
rm -f mkfifo4.c
cd $odir

fifo=/tmp/mkfifo4.fifo
trap "rm -f $fifo /tmp/mkfifo4" EXIT INT

export runRUNTIME=5m
(cd ..; ./run.sh disk.cfg) > /dev/null 2>&1 &
sleep .2

while pgrep -fq run.sh; do
	timeout 300 /tmp/mkfifo4 | grep -v Done
	[ $? -eq 124 ] &&
	    { echo "Timedout"; exit 1; }
done
wait

exit $s
EOF
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define ATIME 1
#define MXRETRY 100
#define LOOPS 1000000

volatile int sigs;
char file[] = "/tmp/mkfifo4.fifo";

static void
hand(int i __unused) {	/* handler */
	sigs++;
}

int
main(void)
{
	pid_t pid, hpid;
	struct sigaction sa;
	int e, fd, fd2, i, status;
	int failures, r, retries, w;
	char c;

	sa.sa_handler = hand;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGALRM, &sa, NULL) == -1)
		err(1, "sigaction");

	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGPIPE, &sa, NULL) == -1)
		err(1, "sigaction");

	unlink(file);
	if (mkfifo(file, 0640) == -1)
		err(1, "mkfifo(%s)", file);
	if ((hpid = fork()) == 0) {
		if ((fd2 = open(file, O_WRONLY | O_APPEND)) == -1)
			err(1, "hold open of fifo");
		for (;;)
			pause();
		_exit(0);
	}

	if ((pid = fork()) == 0) {
		setproctitle("child");
		r = 0;
		for (i = 0; i < LOOPS; i++) {
			failures = 0;
restart:
			do {
				if ((fd = open(file, O_RDONLY |
				    O_NONBLOCK)) == -1)
					if (errno != EINTR) /* on OS X */
						err(1, "open(%s, O_RDONLY)",
						    file);
			} while (fd == -1);
			retries = 0;
			do {
				if ((e = read(fd, &c, 1)) == -1) {
					if (errno != EINTR &&
					    errno != EAGAIN)
						err(1, "read(%d, ...)", fd);
				} else if (retries++ > MXRETRY) {
					close(fd);
					usleep(1000);
					fprintf(stderr,
					    "Re-open for read @ %d\n", i);
					if (failures++ > 100)
						errx(1,
						    "FAIL: Failure to read");
					goto restart;
				}
			} while (e <= 0);
			r++;
			ualarm(ATIME, 0);
			if (close(fd) == -1)
				err(1, "close() in child");
			alarm(0);
		}
		fprintf(stdout, "Done  child. %d  reads, %d signals.\n", r,
		    sigs);
		fflush(stdout);
		_exit(0);
	}
	setproctitle("parent");
	w = 0;
	for (i = 0; i < LOOPS; i++) {
		do {
			if ((fd = open(file, O_WRONLY | O_APPEND |
			    O_NONBLOCK)) == -1)
				if (errno != EINTR && errno != ENXIO)
					err(1, "open(%s, O_WRONLY)", file);
		} while (fd == -1);
		do {
			if ((e = write(fd, "a", 1)) != 1)
				if (errno != EPIPE && errno != EAGAIN)
					err(1, "write(%d, ...)", fd);
		} while (e == -1);
		w++;
		ualarm(ATIME, 0);
		if (close(fd) == -1)
			err(1, "close() in parent");
		alarm(0);
	}
	fprintf(stdout, "Done parent. %d writes, %d signals.\n", w, sigs);

	if (waitpid(pid, &status, 0) == -1)
		err(1, "wait");
	if (kill(hpid, SIGHUP) == -1)
		err(1, "kill %d", hpid);
	if (waitpid(hpid, NULL, 0) == -1)
		err(1, "wait");
	if (unlink(file) == -1)
		err(1, "unlink(%s)", file);

	return (WEXITSTATUS(status));
}

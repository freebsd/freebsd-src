#!/bin/sh

#
# Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
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

# Regression test. Causes panic on 6.1

. ../default.cfg

odir=`pwd`
dir=/tmp

cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/revoke.c
mycc -o revoke -Wall revoke.c || exit 1
rm -f revoke.c

n=100	# Number of times to test
for i in `jot $n`; do
   ./revoke /dev/ttyv9 > /dev/null 2>&1 &
   ./revoke /dev/ttyva > /dev/null 2>&1 &
   ./revoke /dev/ttyvb > /dev/null 2>&1 &
   ./revoke /dev/ttyvc > /dev/null 2>&1 &
   wait
done

rm -f revoke

exit
EOF
/* By Martin Blapp, <mb@imp.ch> <mbr@FreeBSD.org> */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <string.h>

/*#define TTY "/dev/ttyv9"*/	/* should be totally unused */
#define CTTY "/dev/tty"

int
main(int argc, char **argv)
{
	int		ttyfd;
	pid_t		pid;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s /dev/ttyv?\n", argv[0]);
		return 1;
	}

	/* Get rid of my ctty. */
	printf("Parent starting: pid %d\n", getpid());
	pid = fork();
	if (pid < 0) {
		err(1, "fork");
		exit(1);
	} else if (pid > 0) {
		int		status;
		/* parent */
		waitpid(pid, &status, 0);
		exit(0);
	}
	/* child */
	printf("Child: pid %d\n", getpid());

	if (setsid() < 0) {
		err(1, "setsid");
		exit(1);
	}
	ttyfd = open(argv[1], O_RDWR);
	if (ttyfd < 0) {
		err(1, "open(%s)", argv[1]);
		exit(1);
	}
	if (ioctl(ttyfd, TIOCSCTTY) < 0) {
		err(1, "ioctl(TIOCSCTTY)");
		exit(1);
	}
	if (revoke(argv[1]) < 0) {
		err(1, "revoke(%s)", argv[1]);
		exit(1);
	}
	if (open(CTTY, O_RDWR) < 0) {
		err(1, "open(%s)", CTTY);
		exit(1);
	}
	return 0;
}

#!/bin/sh

#
# Copyright (c) 2016 EMC Corp.
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

# "panic: Assertion !tty_gone(tp) failed at ttydevsw.h:153" seen.
# https://people.freebsd.org/~pho/stress/log/posix_openpt2.txt
# Fixed by r312077

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > posix_openpt2.c
mycc -o posix_openpt2 -Wall -Wextra -O2 posix_openpt2.c -lutil || exit 1
rm -f posix_openpt2.c

/tmp/posix_openpt2 &

while kill -0 $! 2>/dev/null; do
	$here/../testcases/swap/swap -t 2m -i 20
done
wait

rm -f /tmp/posix_openpt2
exit 0
EOF
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <libutil.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define LOOPS 10
#define PARALLEL 2
#define RUNTIME 300

void
churn(char *path)
{
	FTS *fts;
	FTSENT *p;
	time_t start;
	int fd, ftsoptions;
	char *args[2];

	ftsoptions = FTS_PHYSICAL;
	args[0] = path;
	args[1] = 0;

	setproctitle("churn");
	start = time(NULL);
	while (time(NULL) - start < RUNTIME / LOOPS) {
		if ((fts = fts_open(args, ftsoptions, NULL)) == NULL)
			err(1, "fts_open");

		while ((p = fts_read(fts)) != NULL) {
			if (p->fts_info == FTS_D ||
			   p->fts_info == FTS_DP)
				continue;
			if ((fd = open(p->fts_path, O_RDONLY)) > 0)
				close(fd);

		}

		if (errno != 0 && errno != ENOENT)
			err(1, "fts_read");
		if (fts_close(fts) == -1)
			err(1, "fts_close()");
	}

	_exit(0);
}

void
pty(void)
{
	time_t start;
        int masterfd, slavefd;
	char *slave;

	start = time(NULL);
	while (time(NULL) - start < RUNTIME / LOOPS) {
		if ((masterfd = posix_openpt(O_RDWR | O_NOCTTY)) == -1)
			err(1, "posix_openpt");
		if ((slave = ptsname (masterfd)) == NULL)
			err(1, "ptsname");
		if ((slavefd = open(slave, O_RDWR|O_NOCTTY)) == -1)
			err(1, "open(%s)", slave);
		usleep(arc4random() % 10000);
		close(slavefd);
		close(masterfd);
	}
	_exit(0);
}

int
main(void)
{
	int i, j;

	for (j = 0; j < LOOPS; j++) {
		for (i = 0; i < PARALLEL; i++) {
			if (fork() == 0)
				pty();
		}
		for (i = 0; i < PARALLEL; i++) {
			if (fork() == 0)
				churn("/dev/pts");
		}
		for (i = 0; i < 2 * PARALLEL; i++)
			wait(NULL);
	}

	return (0);
}

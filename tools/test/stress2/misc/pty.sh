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

# pty(4) test scenario.

# "panic: make_dev_credv: bad si_name (error=17, si_name=ptysn)" seen.
# https://people.freebsd.org/~pho/stress/log/pty.txt

# /dev/pty[l-sL-S][0-9a-v]  Pseudo-terminal master devices.
# /dev/tty[l-sL-S][0-9a-v]  Pseudo-terminal slave devices.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

kldstat -v | grep -q pty || { kldload pty || exit 0; }

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > pty.c
mycc -o pty -Wall -Wextra -O2 pty.c || exit 1
rm -f pty.c

su $testuser -c /tmp/pty

rm -f /tmp/pty
exit
EOF
#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define PARALLEL 4
#define RUNTIME 300

char path[80];

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
	while (time(NULL) - start < RUNTIME) {
		if ((fts = fts_open(args, ftsoptions, NULL)) == NULL)
			err(1, "fts_open");

		while ((p = fts_read(fts)) != NULL) {
			if (p->fts_info == FTS_D ||
			   p->fts_info == FTS_DP)
				continue;
			if ((fd = open(p->fts_path, O_RDWR)) == -1)
				if ((fd = open(p->fts_path, O_WRONLY)) == -1)
					if ((fd = open(p->fts_path, O_RDONLY)) == -1)
						continue;
			usleep(arc4random() % 1000);
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
	int fd[512], i, j, n;
	char c1, c2;

	n = 0;
	c1 = 'l';
	for (i = 0; i < 16; i++) {
		c2 = '0';
		for (j = 0; j < 32; j++) {
			snprintf(path, sizeof(path), "/dev/pty%c%c", c1, c2);
			fd[n++] = open(path, O_RDWR);
			if (c2 == '9')
				c2 = 'a';
			else
				c2++;
		}
		if (c1 == 's')
			c1 = 'L';
		else
			c1++;
	}

	for (i = 0; i < n; i++)
		if (fd[i] != -1)
			close(fd[i]);
}

int
main(void)
{
	int i;

	time_t start;

	for (i = 0; i < PARALLEL; i++)
		if (fork() == 0)
			churn("/dev");

	start = time(NULL);
	while (time(NULL) - start < RUNTIME)
		pty();

	for (i = 0; i < PARALLEL; i++)
		wait(NULL);

	return (0);
}

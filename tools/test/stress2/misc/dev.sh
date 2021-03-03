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

# Test parallel read access to /dev.

# "panic: Most recently used by DEVFS1" seen.
# https://people.freebsd.org/~pho/stress/log/dev.txt
# Fixed by r293826.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > dev.c
mycc -o dev -Wall -Wextra -O2 dev.c || exit 1
rm -f dev.c

echo "Expect: g_access(958): provider ufsid/5c581221fcac7d80 has error 6 set"
daemon sh -c \
    "(cd $here/../testcases/swap; ./swap -t 6m -i 20 -k -l 100)" > \
    /dev/null

/tmp/dev	# Note: this runs as root.
s=$?

while pkill -9 swap; do
	:
done

rm -f /tmp/dev
exit $s
EOF
#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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

	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		if ((fts = fts_open(args, ftsoptions, NULL)) == NULL)
			err(1, "fts_open");

		while ((p = fts_read(fts)) != NULL) {
			if (p->fts_info == FTS_D ||
			   p->fts_info == FTS_DP)
				continue;
			if (strstr(p->fts_path, "ttyu") != NULL)
				continue;
			if (strstr(p->fts_path, "fcdm") != NULL)
				continue;
			if ((fd = open(p->fts_path, O_RDONLY|O_NONBLOCK)) == -1)
				continue;
			usleep(arc4random() % 1000);
			close(fd);

		}

		if (errno != 0 && errno != ENOENT)
			warn("fts_read");
		if (fts_close(fts) == -1)
			err(1, "fts_close()");
	}

	_exit(0);
}

int
main(void)
{
	int e, i, status;;

	e = 0;
	for (i = 0; i < PARALLEL; i++)
		if (fork() == 0)
			churn("/dev");

	for (i = 0; i < PARALLEL; i++) {
		wait(&status);
		if (status != 0)
			e++;
	}

	return (e);
}

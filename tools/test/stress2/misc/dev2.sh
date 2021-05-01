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

# Test parallel access to /dev. A non-root version.

# "panic: Bad link elm 0xfffff8000b07ed00 prev->next != elm" seen.
# https://people.freebsd.org/~pho/stress/log/dev2.txt

# Fixed by r294204.

# "panic: Assertion !tty_gone(tp) failed at ../sys/ttydevsw.h:165" seen:
# https://people.freebsd.org/~pho/stress/log/dev2-2.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

kldstat -v | grep -q pty || { kldload pty || exit 0; }
here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > dev2.c
mycc -o dev2 -Wall -Wextra -O2 dev2.c || exit 1
rm -f dev2.c

daemon sh -c \
    "(cd $here/../testcases/swap; ./swap -t 6m -i 20 -k -l 100)" > \
    /dev/null

su $testuser -c /tmp/dev2

while pkill -9 swap; do
	:
done

rm -f /tmp/dev2
exit
EOF
#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define PARALLEL 4
#define RUNTIME 300

jmp_buf jbuf;
char path[80];

void
handler(int i __unused) {
        longjmp(jbuf, 1);
}

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

		(void)setjmp(jbuf);
		ualarm(0, 0);
		while ((p = fts_read(fts)) != NULL) {
			if (p->fts_info == FTS_D ||
			   p->fts_info == FTS_DP)
				continue;
			ualarm(500000, 0);
			if ((fd = open(p->fts_path, arc4random() % (O_CLOEXEC << 2))) == -1)
				continue;
			ualarm(0, 0);
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
	int i;

	signal(SIGALRM, handler);
	for (i = 0; i < PARALLEL; i++)
		if (fork() == 0)
			churn("/dev");

	for (i = 0; i < PARALLEL; i++)
		wait(NULL);

	return (0);
}

#!/bin/sh

#
# Copyright (c) 2012 Peter Holm <pho@FreeBSD.org>
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

# procfs(4) test scenario.
# "panic: wchan 0xc10a4f68 has no wmesg" seen

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

mount | grep -q "/proc " || { mount -t procfs procfs /proc || exit 1; }
here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > procfs3.c
mycc -o procfs3 -Wall -Wextra -O2 procfs3.c || exit 1
rm -f procfs3.c
cd $here

su $testuser -c /tmp/procfs3

rm -f /tmp/procfs3
exit 0
EOF
#include <sys/param.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <libutil.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PARALLEL 10

void
handler(int i __unused)
{
}

int
test(void)
{

	FTS		*fts;
	FTSENT		*p;
	int		ftsoptions;
	char		*args[2];
	int fd, i;
	char buf[1629];

	ftsoptions = FTS_PHYSICAL;
	args[0] = "/proc";
	args[1] = 0;

	if ((fts = fts_open(args, ftsoptions, NULL)) == NULL)
		err(1, "fts_open");

	while ((p = fts_read(fts)) != NULL) {
		switch (p->fts_info) {
			case FTS_F:			/* Ignore. */
				break;
			case FTS_D:			/* Ignore. */
				continue;
			case FTS_DP:
				continue;
			case FTS_DC:			/* Ignore. */
				continue;
			case FTS_SL:			/* Ignore. */
				continue;
			case FTS_DNR:
				continue;
			case FTS_NS:
				continue;
			case FTS_ERR:
			case FTS_DEFAULT:
				warnx("%s: %s. fts_info = %d", p->fts_path, strerror(p->fts_errno),
				    p->fts_info);
				continue;
			default:
				printf("%s: default, %d\n", getprogname(), p->fts_info);
				break;
		}

		if ((fd = open(p->fts_path, O_RDONLY)) == -1)
			continue;
		signal(SIGALRM, handler);
		alarm(1);

		for (i = 0; i < 2; i++) {
			read(fd, buf, 1629);
		}
		close(fd);
	}

	if (errno != 0 && errno != ENOENT)
		err(1, "fts_read");
	if (fts_close(fts) == -1)
		err(1, "fts_close()");

	return (0);
}

int
main(void)
{
	int i, j;

	for (i = 0; i < PARALLEL; i++) {
		if (fork() == 0) {
			for (j = 0; j < 50; j++) {
				test();
			}
			_exit(0);
		}
	}

	for (i = 0; i < PARALLEL; i++)
		wait(NULL);

	return (0);
}

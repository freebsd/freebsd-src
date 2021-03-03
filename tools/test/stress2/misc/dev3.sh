#!/bin/sh

#
# Copyright (c) 2017 Dell EMC Isilon
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

# pts memory leak regression test.

# Leaks seen when flags is either O_SHLOCK or O_EXLOCK and /dev/ptmx and
# /dev/pts/ is being opened.
# Fixed in r313496.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

kldstat -v | grep -q pty || { kldload pty || exit 0; }
here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > dev3.c
mycc -o dev3 -Wall -Wextra -O2 dev3.c || exit 1
rm -f dev3.c

#(cd $here/../testcases/swap; ./swap -t 10h -i 20 -l 100) > \
#    /dev/null &

pts=`vmstat -m | grep pts | awk '{print $2}'`
[ -z "$pts" ] && pts=0

e=0
n=0
while true; do
	su $testuser -c "/tmp/dev3 $n"
	new=`vmstat -m | grep pts | awk '{print $2}'`
	if [ $new -gt $pts ]; then
		leak=$((new - pts))
		printf "flag %d (0x%x) leaks %d pts, %d allocated.\n" $n $n \
		    $leak $new
		pts=$new
		e=1
	fi
	[ $n -eq 0 ] && n=1 || n=$((n * 2))
	[ $n -gt $((0x00200000)) ] && break	# O_VERIFY
done
while pkill -9 swap; do
	sleep 1
done
wait
rm -f /tmp/dev3
exit $e
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
#define RUNTIME 60

jmp_buf jbuf;
char path[80];

void
handler(int i __unused) {
        longjmp(jbuf, 1);
}

void
churn(int flag, char *path)
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
			if ((fd = open(p->fts_path, flag)) == -1)
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
main(int argc, char *argv[])
{
	int flag, i;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <flag>\n", argv[0]);
		exit(1);
	}
	flag = atoi(argv[1]);
	signal(SIGALRM, handler);
	for (i = 0; i < PARALLEL; i++)
		if (fork() == 0)
			churn(flag, "/dev");

	for (i = 0; i < PARALLEL; i++)
		wait(NULL);

	return (0);
}

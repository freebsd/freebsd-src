#!/bin/sh

#
# Copyright (c) 2013 EMC Corp.
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

# Regression test for r236822.
# http://people.freebsd.org/~pho/stress/log/fdgrowtable.txt
# Fixed in r256210.

. ../default.cfg

max=`ulimit -n`

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > fdgrowtable.c
mycc -o fdgrowtable -Wall -Wextra -O2 fdgrowtable.c || exit 1
rm -f fdgrowtable.c
cd $here

su $testuser -c "/tmp/fdgrowtable $max" &
while kill -0 $! 2>/dev/null; do
	../testcases/swap/swap -t 2m -i 40 -h
done
wait
rm -f /tmp/fdgrowtable
exit

EOF
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define PARALLEL 3

int max;

void test(void)
{
	int i;

	for (i = 0; i < max; i++) {
		if (dup2(1, i + 3) == -1)
			err(1, "dup2(%d)", i + 3);
	}
	_exit(0);
}

int
main(int argc, char **argv)
{
	time_t start;
	int i;

	if (argc == 2)
		max = atoi(argv[1]);
	else
		err(1, "Usage: %s <maxfiles>", argv[0]);

	max = (max - 3) / PARALLEL;

	start = time(NULL);
	while (time(NULL) - start < 600) {
		for (i = 0; i < PARALLEL; i++) {
			if (fork() == 0)
				test();
		}

		for (i = 0; i < PARALLEL; i++)
			wait(NULL);
	}

	return (0);
}

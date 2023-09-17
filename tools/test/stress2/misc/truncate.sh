#!/bin/sh

#
# Copyright (c) 2009 Peter Holm <pho@FreeBSD.org>
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

# Test high water mark for freeblks

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > truncate.c
mycc -o truncate -Wall -Wextra -O2 truncate.c || exit 1
rm -f truncate.c
[ -d $RUNDIR ] || mkdir -p $RUNDIR
cd $RUNDIR

/tmp/truncate

rm -f /tmp/truncate
exit 0
EOF
#include <fcntl.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

#define SIZ 512
char buf[SIZ];

void
test(void)
{
	time_t start;
	int fd[10], i, j;
	char name[128];

	for (j = 0; j < 10; j++) {
		sprintf(name, "%05d.%05d", getpid(), j);
		if ((fd[j] = open(name, O_WRONLY | O_CREAT | O_APPEND, 0666)) == -1)
			err(1, "%s", name);
		unlink(name);
	}

	start = time(NULL);
	for (i = 0; i < 100000; i++) {
		if (time(NULL) - start > 1200)
			break;
		for (j = 0; j < 10; j++) {
			if (write(fd[j], buf, 2) != 2)
				err(1, "write");
			if (ftruncate(fd[j], 0) == -1)
				err(1, "ftruncate");
		}
	}

	_exit(0);
}

int
main(void)
{
	int i, status;

	for (i = 0; i < 20; i++) {
		if (fork() == 0)
			test();
	}
	for (i = 0; i < 20; i++)
		wait(&status);

	return (0);
}

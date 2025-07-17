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

# "panic: make_dev_sv: bad si_name (error=17, si_name=ptyp0)" seen.
# Fixed by r293825.

# Based on test scenario by Bruce Evans.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

kldstat -v | grep -q pty || { kldload pty || exit 0; }

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > pty2.c
mycc -o pty2 -Wall -Wextra -O2 pty2.c || exit 1
rm -f pty2.c

/tmp/pty2

rm -f /tmp/pty2
exit
EOF
#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define PARALLEL 3
#define RUNTIME 180

#define IN "/dev/ptyp0"

void
test(void)
{
	time_t start;
	int fd;

	start = time(NULL);
	while (time(NULL) - start < RUNTIME) {
		if ((fd = open(IN, O_RDONLY)) == -1) {
			if (errno != EBUSY && errno != ENXIO && errno != ENOENT)
				err(1, "open(%s)", IN);
		} else
			close(fd);

	}
	_exit(0);
}

int
main(void)
{
	int i;

	for (i = 0; i < PARALLEL; i++)
		if (fork() == 0)
			test();

	for (i = 0; i < PARALLEL; i++)
		wait(NULL);

	return(0);
}


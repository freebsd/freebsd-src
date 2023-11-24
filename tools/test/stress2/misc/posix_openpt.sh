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

# "panic: Assertion ttyinq_getsize(&tp->t_inq) == 0 failed" seen.
# https://people.freebsd.org/~pho/stress/log/posix_openpt.txt
# Test scenario by brde@

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > posix_openpt.c
mycc -o posix_openpt -Wall -Wextra -O2 posix_openpt.c || exit 1
rm -f posix_openpt.c

for i in `jot 10`; do
	/tmp/posix_openpt &
done

me=`tty`
stty -f /dev/ptmx 300 2>/dev/null
for i in /dev/pts/*; do
	[ $i = $me ] && continue
	stty -f $i 300 2>/dev/null
done
wait
rm -f /tmp/posix_openpt

exit
EOF
#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

int
main(void)
{
	int masterfd, slavefd;
	char *slave;

	if ((masterfd = posix_openpt(O_RDWR | O_NOCTTY)) == -1)
		err(1, "posix_openpt");
	if ((slave = ptsname (masterfd)) == NULL)
		err(1, "ptsname");
	if ((slavefd = open(slave, O_RDWR|O_NOCTTY)) == -1)
		err(1, "open(%s)", slave);

	sleep(arc4random() % 60 + 1);

	return (0);
}

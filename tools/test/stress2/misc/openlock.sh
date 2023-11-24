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

# Check O_EXLOCK behaviour on different file types
# Regression test for r313549.

# Test scenario by kib@

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

kldstat -v | grep -q pty || { kldload pty || exit 0; }

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/openlock.c
mycc -o openlock -Wall -Wextra -O0 -g openlock.c || exit 1
rm -f openlock.c

e=0
echo "expect: \"error 45 Operation not supported\""
/tmp/openlock /dev/ptmx
s=$?
[ $s -eq 45 ] || { echo "Expected error 45, got $s."; e=1; }
/tmp/openlock /bin/ls
s=$?
[ $s -eq 0 ] || { echo "Expected error 0, got $s."; e=$((e + 2)); }
mkfifo openlock.fifo
s=$?
sleep 1 > openlock.fifo &
/tmp/openlock openlock.fifo
s=$?
[ $s -eq 45 ] || { echo "Expected error 45, got $s."; e=$((e + 4)); }
wait

rm -f /tmp/openlock openlock.fifo
exit $e
EOF

/* $Id: openlock.c,v 1.1 2017/02/10 14:07:24 kostik Exp kostik $ */

#include <sys/fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	int e, fd, i;

	for (i = 1; i < argc; i++) {
		fd = open(argv[i], O_RDONLY | O_EXLOCK);
		if (fd == -1) {
			printf("%s error %d %s\n", argv[i], errno,
			    strerror(errno));
			e = errno;
		} else {
			printf("%s success\n", argv[i]);
			close(fd);
			e = 0;
		}
	}
	return (e);
}


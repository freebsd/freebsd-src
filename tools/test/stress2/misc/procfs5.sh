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

# Issue involving signed overflow.
# Test scenario based on panic seen in
# http://people.freebsd.org/~pho/stress/log/kostik640.txt
# Fixed in r258365, r258397.

# Scenario by kib@

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

mount | grep -q "/proc " || mount -t procfs procfs /proc

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > procfs_lr.c
mycc -o procfs_lr -Wall -Wextra -O2 procfs_lr.c || exit 1
rm -f procfs_lr.c

/tmp/procfs_lr 2>/dev/null

rm -f /tmp/procfs_lr

exit 0
EOF
/* $Id: procfs_lr.c,v 1.1 2013/11/16 07:06:46 kostik Exp kostik $ */

#include <sys/types.h>
#include <sys/fcntl.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

int
main(void)
{
	static const char name[] = "/proc/curproc/map";
	const off_t uio_offset = 0x6f51f3a1185bced9;
#if defined(__LP64__)
	const size_t uio_resid = 0x4c330b10965a61af;
#else
	const size_t uio_resid = 0x965a61af;
#endif
	char buf[1];
	int error, fd;

	fd = open(name, O_RDONLY);
	if (fd == -1)
		err(1, "open");
	error = pread(fd, buf, uio_resid, uio_offset);
	if (error == -1)
		fprintf(stderr, "pread: %s\n", strerror(errno));
	return (0);
}

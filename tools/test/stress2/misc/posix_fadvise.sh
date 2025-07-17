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

# Memory leak. Fixed in r232702.

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > posix_fadvise.c
mycc -o posix_fadvise -Wall -Wextra -O2 -g posix_fadvise.c

n1=`vmstat -m | grep fadvise | awk '{print $2 + 0}'`
/tmp/posix_fadvise
n2=`vmstat -m | grep fadvise | awk '{print $2 + 0}'`
if [ $((n2 - n1)) -gt 10 ]; then
	echo FAIL
	vmstat -m | sed -n '1p;/fadvise/p'
fi

rm -f /tmp/posix_fadvise posix_fadvise.c
exit
EOF
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(void)
{
	off_t len, offset;
	int advise, fd, i;

	for (i = 0; i < 500; i++) {
		if ((fd = open("posix_fadvise.c", O_RDONLY)) == -1)
			err(1, "open()");
		offset = arc4random();
		len = arc4random();
		advise = arc4random() % 6;
		if (posix_fadvise(fd, offset, len, advise) == -1)
			warn("posix_fadvise");
		close(fd);
	}
	return (0);
}

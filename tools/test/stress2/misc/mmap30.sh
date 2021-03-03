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

# r320344: allow mprotect(2) over the guards to succeed regardless of
# the requested protection.

. ../default.cfg

grep -q MAP_GUARD /usr/include/sys/mman.h 2>/dev/null || exit 0
here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > mmap30.c
mycc -o mmap30 -Wall -Wextra -O2 -g mmap30.c || exit 1
rm -f mmap30.c /tmp/mmap30.core

/tmp/mmap30 > /dev/null
s=$?

rm -f /tmp/mmap30 /tmp/mmap30.core
exit $s
EOF
#include <sys/types.h>
#include <sys/mman.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(void)
{
	int pagesz;
	void *addr;

	pagesz = getpagesize();
	addr = mmap(NULL, pagesz, PROT_NONE, MAP_GUARD, -1, 0);
	if (addr == (char *)MAP_FAILED)
		err(1, "FAIL: mmap(MAP_GUARD)");

	if (mprotect(addr, pagesz, PROT_READ | PROT_WRITE) == -1)
		err(1, "mprotect(RW)");

	if (mprotect(addr, pagesz, PROT_NONE) == -1)
		err(1, "mprotect(RW)");

	if (munmap(addr, pagesz) == -1)
		err(1, "munmap");

	return (0);
}

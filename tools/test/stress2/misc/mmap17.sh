#!/bin/sh

#
# Copyright (c) 2014 EMC Corp.
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

# Test scenario by kib@

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > mmap17.c
mycc -o mmap17 -Wall -Wextra -O2 -g mmap17.c -lpthread || exit 1
rm -f mmap17.c /tmp/mmap17.core
rm -f /tmp/mmap17.core

{ /tmp/mmap17 > /dev/null; } 2>&1 | grep -v worked

rm -f /tmp/mmap17 /tmp/mmap17.core
exit 0
EOF
/* $Id: map_excl.c,v 1.2 2014/06/16 06:02:52 kostik Exp kostik $ */

#include <sys/mman.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef MAP_EXCL
#define	MAP_EXCL	 0x00004000 /* for MAP_FIXED, fail if address is used */
#endif

int
main(void)
{
	char *addr, *addr1;
	int pagesz;
	char cmd[128];

	pagesz = getpagesize();
	addr = mmap(NULL, pagesz, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);
	if (addr == (char *)MAP_FAILED)
		err(1, "mmap 1");
	printf("addr %p\n", addr);

	addr -= pagesz;
	addr1 = mmap(addr, 3 * pagesz, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_FIXED | MAP_EXCL, -1, 0);
	if (addr1 == MAP_FAILED)
		warn("EXCL worked");
	else
		fprintf(stderr, "EXCL failed\n");

	addr1 = mmap(addr, 3 * pagesz, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_FIXED, -1, 0);
	printf("addr1 %p\n", addr);

	snprintf(cmd, sizeof(cmd), "procstat -v %d", getpid());
	system(cmd);

	return (0);
}

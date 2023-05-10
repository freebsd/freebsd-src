#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021 Peter Holm <pho@FreeBSD.org>
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

# Test scenario suggestion by alc@

. ../default.cfg

cd /tmp
cat > mprotect2.c <<EOF
/*
    For example, write a program that mmap(MAP_ANON)'s a gigabyte of
    PROT_WRITE virtual address space, iterates over that space removing
    PROT_WRITE on every other page, and then measure the time to perform a
    single mprotect() restoring PROT_WRITE to the entire gigabyte of virtual
    address space.
 */

#include <sys/param.h>
#include <sys/mman.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SIZ 0x40000000LL	/* 1GB */

static void
test(void)
{
	size_t i, len;
	char *cp;

	len = SIZ;
	if ((cp = mmap(NULL, len, PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0))
	    == MAP_FAILED)
		err(1, "mmap");

	for (i = 0; i < SIZ; i += PAGE_SIZE * 2) {
		if (mprotect(cp + i, PAGE_SIZE, 0) != 0)
			err(1, "mprotect(). %d", __LINE__);
	}
	if (mprotect(cp, SIZ, PROT_WRITE) != 0)
		err(1, "mprotect(). %d", __LINE__);

	if (munmap(cp, SIZ) == -1)
		err(1, "munmap()");
}

int
main(void)
{
	int i;

	/* Slow run with debug.vmmap_check=1 */
	alarm(120);

	for (i = 0; i < 64; i++)
		test();

	return (0);
}
EOF
mycc -o mprotect2 -Wall -Wextra -O2 mprotect2.c || exit 1

./mprotect2; s=$?

rm mprotect2.c mprotect2
exit $s

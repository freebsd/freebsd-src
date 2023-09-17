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

# "panic: vm_map_protect: inaccessible wired map entry" seen:
# http://people.freebsd.org/~pho/stress/log/mmap11.txt
# Fixed in r266780.

# Test scenario by: Mark Johnston markj@

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/mmap12.c
mycc -o mmap12  -O2 -Wall -Wextra mmap12.c || exit 1
rm -f mmap12.c
cd $odir

/tmp/mmap12

rm -f /tmp/mmap12
exit

EOF
#include <sys/mman.h>

#include <err.h>
#include <string.h>
#include <unistd.h>

int
main(void)
{
	void *addr;
	size_t sz = 1;

/*
 * This is the minimum amount of C code ot takes to panic the kernel.
 * This is as submitted and thus not a complete and correct test program.
 */
	addr = mmap(NULL, sz, PROT_READ, MAP_ANON, -1, 0);
	if (addr == NULL)
		err(1, "mmap");

	if (mlock(addr, sz) != 0)
		err(1, "mlock");
	if (mprotect(addr, sz, PROT_NONE) != 0)
		err(1, "mprotect 1");
	if (mprotect(addr, sz, PROT_WRITE) != 0)
		err(1, "mprotect 2");
	if (munmap(addr, sz) != 0)
		err(1, "munmap");

	return (0);
}

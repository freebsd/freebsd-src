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

# "panic: pmap_unwire: pte 0x672d405 is missing PG_W" seen.
# http://people.freebsd.org/~pho/stress/log/mmap20.txt

# Test scenario by: Mark Johnston markj@

# Fixed by r272036

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/mmap20.c
mycc -o mmap20 -O2 -Wall -Wextra mmap20.c || exit 1
rm -f mmap20.c
cd $odir

/tmp/mmap20
s=$?

rm -f /tmp/mmap20
exit $s

EOF
#include <sys/types.h>
#include <sys/mman.h>

#include <err.h>
#include <stdlib.h>

int
main(void)
{
	char *ptr;
	size_t sz;

	sz = 4096;
	ptr = mmap(NULL, sz, PROT_READ, MAP_ANON, -1, 0);
	if (ptr == NULL)
		err(1, "mmap");

	if (mlock(ptr, sz) != 0)
		err(1, "mlock");

	if (mprotect(ptr, sz, PROT_EXEC) != 0)
		err(1, "mprotect");

	if (madvise(ptr, sz, MADV_WILLNEED) != 0)
		err(1, "madvise");

	if (munlock(ptr, sz) != 0)
		err(1, "munlock");

	return (0);
}

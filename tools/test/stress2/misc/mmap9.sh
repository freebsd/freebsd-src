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

# "panic: vm_map_protect: object 0xca6869c0 overcharged" seen:
# http://people.freebsd.org/~pho/stress/log/mmap9.txt
# Fixed in r265843

# Test scenario by: Mark Johnston markj@

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/mmap9.c
# At one point during the fix development, only the thread version would panic
mycc -o mmap9  -O2 -Wall -Wextra mmap9.c           || exit 1
mycc -o mmap9p -O2 -Wall -Wextra mmap9.c -lpthread || exit 1
rm -f mmap9.c
cd $odir

/tmp/mmap9
/tmp/mmap9p

rm -f /tmp/mmap9 /tmp/mmap9p
exit

EOF
#include <sys/types.h>
#include <sys/mman.h>

#include <err.h>
#include <string.h>
#include <unistd.h>

int
main(void)
{
	size_t sz = 1;
	char *addr;

/*
 * This is the minimum amount of C code it takes to panic the kernel.
 * This is as submitted and thus not a complete and correct test program.
 */
	addr = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_ANON,
	    -1, 0);
	if (addr == NULL)
		err(1, "mmap");
	memset(addr, 0, sz);

	if (fork() != 0) {
		if (mlock(addr, sz))
			err(1, "mlock");
		if (mprotect(addr, sz, PROT_READ))
			err(1, "mprotect");
		if (mprotect(addr, sz, PROT_READ | PROT_WRITE))
			err(1, "mprotect");
		if (mprotect(addr, sz, PROT_READ))
			err(1, "mprotect");
		if (mprotect(addr, sz, PROT_READ | PROT_WRITE))
			err(1, "mprotect");
	} else
		/*
		 * Ensure that shadow objects aren't collapsed by process exit.
		 */
		sleep(1);

	return (0);
}

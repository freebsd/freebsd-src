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

# Leak of "vm.stats.vm.v_user_wire_count" seen.
# This test must run in single user mode for accurate leak reporting.
# Test scenario by: Mark Johnston markj@
# Fixed in r269134.

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/mmap13.c
mycc -o mmap13  -O2 -Wall -Wextra mmap13.c || exit 1
rm -f mmap13.c
cd $odir

# Both the 5000 and 500 are empirical values.
# Combined they demonstrate the leak in a consistent way.

v1=`sysctl -n vm.stats.vm.v_user_wire_count`
for i in `jot 5000`; do
	/tmp/mmap13
done 2>&1 | tail -5
v2=`sysctl -n vm.stats.vm.v_user_wire_count`
[ $v2 -gt $((v1 + 500)) ] &&
    echo "vm.stats.vm.v_user_wire_count changed from $v1 to $v2."

rm -f /tmp/mmap13
exit 0

EOF
#include <sys/mman.h>

#include <err.h>
#include <stdlib.h>
#include <unistd.h>

int
main(void)
{
	void *addr;
	size_t sz;

	sz = getpagesize();
	addr = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);
	if (addr == NULL)
		err(1, "mmap");
	if (mlock(addr, sz) != 0)
		err(1, "mlock");
	if (mprotect(addr, sz, PROT_NONE) != 0)
		err(1, "mprotect");

	return (0);
}

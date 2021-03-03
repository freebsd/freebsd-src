#!/bin/sh

#
# Copyright (c) 2018 Dell EMC Isilon
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

# msetdomain(2) fuzz test.
# No problems seen.

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

nm /usr/lib/libc.a | grep -q __sys_msetdomain || exit 0
dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/msetdomain.c
mycc -o msetdomain -Wall -Wextra -O0 -g msetdomain.c || exit 1
rm -f msetdomain.c
cd $odir

$dir/msetdomain
s=$?
[ -f msetdomain.core -a $s -eq 0 ] &&
    { ls -l msetdomain.core; mv msetdomain.core /tmp; s=1; }
rm -rf $dir/msetdomain
exit $s

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/cpuset.h>
#include <sys/domainset.h>

#include <err.h>
#include <stdlib.h>
#include <time.h>

/*
struct msetdomain_args {
        void            *addr;
        size_t          size;
        size_t          domainsetsize;
        domainset_t     *mask;
        int             policy;
        int             flags;
*/

static long
random_long(long mi, long ma)
{
        return (arc4random()  % (ma - mi + 1) + mi);
}

void
flip(void *ap, size_t len)
{
	unsigned char *cp;
	int byte;
	unsigned char bit, buf, mask, old;

	cp = (unsigned char *)ap;
	byte = random_long(0, len);
	bit = random_long(0,7);
	mask = ~(1 << bit);
	buf = cp[byte];
	old = cp[byte];
	buf = (buf & mask) | (~buf & ~mask);
	cp[byte] = buf;
}

int
main(void)
{
	size_t len;
	time_t start;
	void *share;
	domainset_t rootmask;
	int flags, policy;

	len = PAGE_SIZE;
	if ((share = mmap(NULL, len, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	start = time(NULL);
	while (time(NULL) - start < 60) {
		if (cpuset_getdomain(CPU_LEVEL_ROOT, CPU_WHICH_PID, -1,
		    sizeof(rootmask), &rootmask, &policy) != 0)
			err(EXIT_FAILURE, "getdomain");

		flags = 0;
		flip(&flags, sizeof(flags));
		msetdomain(share, len, sizeof(rootmask), &rootmask, policy,
		    flags);
	}

	start = time(NULL);
	while (time(NULL) - start < 60) {
		if (cpuset_getdomain(CPU_LEVEL_ROOT, CPU_WHICH_PID, -1,
		    sizeof(rootmask), &rootmask, &policy) != 0)
			err(EXIT_FAILURE, "getdomain");

		flip(&policy, sizeof(policy));
		msetdomain(share, len, sizeof(rootmask), &rootmask, policy,
		    flags);
	}

	start = time(NULL);
	while (time(NULL) - start < 60) {
		if (cpuset_getdomain(CPU_LEVEL_ROOT, CPU_WHICH_PID, -1,
		    sizeof(rootmask), &rootmask, &policy) != 0)
			err(EXIT_FAILURE, "getdomain");

		flip(&rootmask, sizeof(rootmask));
		msetdomain(share, len, sizeof(rootmask), &rootmask, policy,
		    flags);
	}

	return (0);
}

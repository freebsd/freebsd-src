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

# Copy of swap.sh, but without a swap disk and using 110% of hw.usermem
# All memory is in active or in laundry, and there are several processes
# swapped out:
# https://people.freebsd.org/~pho/stress/log/kostik1068.txt

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/swap4.c
mycc -o swap4 -Wall -Wextra -O2 swap4.c || exit 1
rm -f swap4.c

usermem=`sysctl hw.usermem | sed 's/.* //'`
size=$((usermem/10*11))

[ `sysctl -n vm.swap_total` -gt 0 ] && { swapoff -a; off=1; }
echo "Expect: swap4, uid 0, was killed: out of swap space"
$dir/swap4 $((size / 4096)) &
sleep .5
while [ `pgrep swap4 | wc -l` -eq 11 ]; do sleep 5; done
pkill swap4
wait
[ $off ] && swapon -a

rm -f $dir/swap4
exit 0
EOF
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define INCARNATIONS 10
#define RUNTIME (5 * 60)

static unsigned long size, original;

static void
setup(void)
{
	struct rlimit rlp;

	size = size / INCARNATIONS;
	original = size;
	if (size == 0)
		errx(1, "Argument too small");

	if (getrlimit(RLIMIT_DATA, &rlp) < 0)
		err(1,"getrlimit");
	rlp.rlim_cur -= 1024 * 1024;

	if (size > (unsigned long)rlp.rlim_cur)
		size = rlp.rlim_cur;

	if (size == 0)
		errx(1, "Argument too small");

	return;
}

static int
test(void)
{
	time_t start;
	unsigned long i, j;
	int page;
	volatile char *c;

	c = malloc(size);
	while (c == NULL) {
		size -=  1024 * 1024;
		c = malloc(size);
	}
	if (size != original)
		printf("Malloc size changed from %ld Mb to %ld Mb\n",
		    original / 1024 / 1024, size / 1024 / 1024);
	page = getpagesize();
	start = time(NULL);
	while ((time(NULL) - start) < RUNTIME) {
		i = j = 0;
		while (i < size) {
			c[i] = 0;
			i += page;
			if (++j % 1024 == 0) {
				if ((time(NULL) - start) >= RUNTIME)
					break;
				if (arc4random() % 100 < 5)
					usleep(1000);
			}
		}
	}
	free((void *)c);

	_exit(0);
}

int
main(int argc, char **argv)
{
	int i;

	if (argc != 2)
		errx(1, "Usage: %s bytes", argv[0]);

	size = atol(argv[1]) * 4096;
	setup();

	for (i = 0; i < INCARNATIONS; i++)
		if (fork() == 0)
			test();

	for (i = 0; i < INCARNATIONS; i++)
		wait(NULL);

	return (0);
}

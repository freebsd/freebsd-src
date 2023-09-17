#!/bin/sh

#
# Copyright (c) 2013 EMC Corp.
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

# Swap test. Variation of swap.sh
# panic: vm_radix_remove: impossible to locate the key
# panic: vm_page_alloc: cache page 0xff... is missing from the free queue
# panic: vm_radix_node_put: rnode 0xfffffe00099035a0 has a child
# panic: vm_radix_remove: impossible to locate the key

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

export RUNDIR=$mntpoint/stressX
rm -rf /tmp/stressX.control
dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/swap2.c
mycc -o swap2 -Wall -Wextra -O2 swap2.c || exit 1
rm -f swap2.c
cd $odir

usermem=`sysctl -n hw.usermem`
swap=`sysctl -n vm.swap_total`

if [ $swap -gt 0 ]; then
	size=$((usermem/10*11))
else
	size=$((usermem/10*8))
fi

/tmp/swap2 $((size / 4096)) &
sleep 30
su $testuser -c "(cd ../testcases/rw; ./rw -t 3m -i 40 -l 100 -v -h -h)" &
wait

while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/swap2
exit
EOF
#include <sys/types.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#define RUNTIME (5 * 60)
#define INCARNATIONS 32

static unsigned long size, original;

void
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

#if 0
	printf("setup: pid %d. Total %luMb\n",
		getpid(), size / 1024 / 1024 * INCARNATIONS);
#endif

	if (size == 0)
		errx(1, "Argument too small");

	return;
}

int
test(void)
{
	volatile char *c;
	int page;
	unsigned long i, j;
	time_t start;

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

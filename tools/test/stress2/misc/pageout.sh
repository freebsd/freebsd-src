#!/bin/sh

#
# Copyright (c) 2015 EMC Corp.
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

# Trigger the two EDEADLK in vm/vm_pageout.c
# OOVM deadlock seen
# https://people.freebsd.org/~pho/stress/log/pageout.txt

# "panic: handle_written_filepage: not started" seen:
# https://people.freebsd.org/~pho/stress/log/pageout-2.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/pageout.c
mycc -o pageout -Wall -Wextra -g pageout.c || exit 1
rm -f pageout.c
cd $odir

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 2g -u $mdstart || exit 1

newfs $newfs_flags md$mdstart > /dev/null

mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

f1=$mntpoint/f1
dd if=/dev/zero of=$f1 bs=1m count=1k status=none

daemon sh -c "(cd ../testcases/swap; ./swap -t 5m -i 20 -l 100 -h)" > /dev/null
(cd /tmp; /tmp/pageout $f1) &
sleep .2
while kill -0 $! 2> /dev/null; do
	mksnap_ffs $mntpoint $mntpoint/.snap/stress2 &&
	    rm -f $mntpoint/.snap/stress2
done
while pgrep -q swap; do
	pkill swap
done
wait

while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/pageout /tmp/pageout.core
exit

EOF
#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

const char *file;

#define RUNTIME 600

void
test(void)
{
	struct stat st;
	size_t i, len;
	time_t start;
	int error, fd, ps;
	char *p;

	ps = getpagesize();
	if ((fd = open(file, O_RDWR)) == -1)
		err(1, "open(%s)", file);
	if ((error = fstat(fd, &st)) == -1)
		err(1, "stat(%s)", file);
	len = round_page(st.st_size);
	do {
		if ((p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED,
		    fd, 0)) == MAP_FAILED) {
			if (errno == ENOMEM)
				len -= ps;
			else
				err(1, "mmap");
		}
	} while (p == MAP_FAILED);

	start = time(NULL);
	/* Touch all pages of the file. */
	for (i = 0; i < len; i += ps)
		p[i] = 1;
	while (time(NULL) - start < RUNTIME)
		p[arc4random() % len] = 1;

	if (munmap(p, len) == -1)
		err(1, "unmap()");
	close(fd);
}

int
main(int argc, char *argv[])
{
	if (argc != 2)
		errx(1, "Usage: %s <file>", argv[0]);
	file = argv[1];

	test();

	return (0);
}

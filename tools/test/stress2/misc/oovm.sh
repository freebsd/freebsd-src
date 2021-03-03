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

# Out of VM deadlock seen. Introduced by r285808.
# https://people.freebsd.org/~pho/stress/log/oovm.txt
# https://people.freebsd.org/~pho/stress/log/oovm-2.txt

# Fixed by r290047 and <alc's PQ_LAUNDRY patch>

# Test scenario suggestion by alc@

. ../default.cfg

[ `swapinfo | wc -l` -eq 1 ] && exit 0
maxsize=$((2 * 1024)) # Limit size due to runtime reasons
size=$((`sysctl -n hw.physmem` / 1024 / 1024))
[ $size -gt $((4 * 1024)) ] &&
    echo "RAM should be capped to 4GB for this test."
[ $size -gt $maxsize ] && size=$maxsize
need=$((size * 2))
d1=$diskimage.1
d2=$diskimage.2
rm -f $d1 $d2
[ `df -k $(dirname $diskimage) | tail -1 | awk '{print int($4 / 1024)}'` -lt \
    $need ] && printf "Need %d MB on %s.\n" $need `dirname $diskimage` && exit
dd if=/dev/zero of=$d1 bs=1m count=$size status=none
cp $d1 $d2 || exit
trap "rm -f $d1 $d2" EXIT INT

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/oovm.c
mycc -o oovm -Wall -Wextra -g oovm.c || exit 1
rm -f oovm.c
cd $odir

(cd /tmp; /tmp/oovm $d1) &
(cd /tmp; /tmp/oovm $d2) &
wait

rm -f /tmp/oovm /tmp/oovm.core
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
	size_t i, olen, len;
	time_t start;
	int error, fd, ps;
	char *p;

	ps = getpagesize();
	if ((fd = open(file, O_RDWR)) == -1)
		err(1, "open(%s)", file);
	if ((error = fstat(fd, &st)) == -1)
		err(1, "stat(%s)", file);
	len = olen = round_page(st.st_size);
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

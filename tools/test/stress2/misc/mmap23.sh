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

# "panic: vm_object_unwire: missing page" seen.
# https://people.freebsd.org/~pho/stress/log/mmap23.txt
# Test scenario by kib@.
# Fixed by r285878.

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/mmap23.c
mycc -o mmap23 -Wall -Wextra mmap23.c || exit 1
rm -f mmap23.c
cd $odir

cp /tmp/mmap23 /tmp/mmap23.inputfile
daemon sh -c '(cd ../testcases/swap; ./swap -t 1m -i 2)' > \
    /dev/null 2>&1

/tmp/mmap23 /tmp/mmap23.inputfile &
sleep .2
dd if=/dev/zero of=/tmp/mmap23.inputfile bs=1k count=1 status=none

while pgrep -q swap; do
	pkill -9 swap
done

rm -f /tmp/mmap23 /tmp/mmap23.inputfile
exit

EOF
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

const char *file;

void
test(void)
{
	struct stat st;
	char *p;
	size_t len;
	int error, fd;

	if ((fd = open(file, O_RDWR)) == -1)
		err(1, "open %s", file);
	if ((error = fstat(fd, &st)) == -1)
		err(1, "stat(%s)", file);
	len = round_page(st.st_size);
	if ((p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED)
		err(1, "mmap");
	p[arc4random() % len] = 1;
	if ((error = mlock(p, len)) == -1)
		err(1, "mlock");
	sleep(2);
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

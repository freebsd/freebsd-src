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

# Verify that vm_fault_dontneed() is called during sequential access of a
# mapped file on a file system.

# Test scenario description by alc@

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
dtrace -n 'dtrace:::BEGIN { exit(0); }' > /dev/null 2>&1 || exit 0

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/vm_fault_dontneed.c
mycc -o vm_fault_dontneed -Wall -Wextra -O0 -g vm_fault_dontneed.c || exit 1
rm -f vm_fault_dontneed.c
cd $odir

size=$((`sysctl -n hw.physmem` / 1024 / 1024))
[ $size -gt $((2 * 1024)) ] &&
    { echo "RAM must be capped to 2GB for this test."
    rm /tmp/vm_fault_dontneed; exit 0; }
need=2048
have=`df -k $(dirname $diskimage) | tail -1 | awk '{print int($4 / 1024)}'`
[ $need -gt $have ] && need=$((have - 1))
dd if=/dev/zero of=$diskimage bs=1m count=$need status=none

log=/tmp/dtrace.$$
trap "rm -f $log" EXIT INT
dtrace -w -n '::*vm_fault_dontneed:entry { @[execname] = count(); }' \
-c "/tmp/vm_fault_dontneed $diskimage" > $log 2>&1

count=`grep vm_fault_dontneed $log | awk 'NF == 2 {print $NF}'`
[ -z "$count" ] && count=0
[ $count -lt 1000 ] &&
    { echo "vm_fault_dontneed count = $count"; s=1; cat $log; }

rm -rf /tmp/vm_fault_dontneed $diskimage
exit $s

EOF
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int
main(int argc __unused, char **argv)
{
	struct stat fs;
	off_t i;
	char *ptr;
	int accum, fd;

	fd = open(argv[1], O_RDONLY);
	if (fd == -1)
		err(1, "open");
	if (fstat(fd, &fs) == -1 || !S_ISREG(fs.st_mode))
		err(1, "fstat");
	ptr = mmap(0, (size_t)fs.st_size, PROT_READ /*| PROT_EXEC*/, MAP_PRIVATE, fd, 0);
	if (ptr == MAP_FAILED)
		err(1, "mmap");
	accum = 0;
	for (i = 0; i < fs.st_size; i++)
		accum += ptr[i];
	if (accum != 0) /* just to trick the optimizer */
		printf("accum: %d\nptr: %p\nsize: %ld\n", accum, ptr, (long)fs.st_size);
}

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

# Test of unmapped unaligned i/o over the vnode-backed md(4) volume.
# "panic: vm_fault: fault on nofault entry, addr: fffffe07f302c000" seen.
# https://people.freebsd.org/~pho/stress/log/md8.txt
# Fixed in r292128.

# Test scenario by kib@

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ -d /usr/src/sys ] || exit 0

. ../default.cfg

rm -f $diskimage
dir=`dirname $diskimage`
free=`df -k $dir | tail -1 | awk '{print $4}'`
[ $((free / 1024)) -lt 50 ] && echo "Not enough disk space." && exit

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > md8.c
rm -f /tmp/md8
mycc -o md8 -Wall -Wextra -g -O2 md8.c || exit 1
rm -f md8.c

cd $odir
trap "rm -f $diskimage" EXIT INT
dd if=/dev/zero of=$diskimage bs=1m count=50 status=none
[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart
mdconfig -a -t vnode -f $diskimage -u $mdstart

n=`sysctl -n hw.ncpu`
n=$((n + 1))
(cd /usr/src; make -j $n buildkernel > /dev/null 2>&1) &
sleep 1
/tmp/md8 /dev/md$mdstart
kill $!
wait

mdconfig -d -u $mdstart
rm -rf /tmp/md8
exit 0
EOF
#include <sys/param.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define LOOPS 2000

void
test(char *path)
{
	int fd;
	char data[MAXPHYS + 512] __aligned(PAGE_SIZE);

	if ((fd = open(path, O_RDONLY)) == -1)
		err(1, "open(%s)", path);
	if (read(fd, data + 512, MAXPHYS) != MAXPHYS)
		err(1, "read");
	close(fd);

	if ((fd = open(path, O_WRONLY)) == -1)
		err(1, "open(%s)", path);
	if (write(fd, data + 512, MAXPHYS) != MAXPHYS)
		err(1, "write");
	close(fd);

	if ((fd = open(path, O_RDONLY)) == -1)
		err(1, "open(%s)", path);
	if (read(fd, data + 512, MAXPHYS) != MAXPHYS)
		err(1, "read");
	close(fd);
}

int
main(int argc, char *argv[])
{
	int i;
	char *path;

	if (argc != 2)
		errx(1, "Usage: %s <path>", argv[0]);

	path = argv[1];

	for (i = 0; i < LOOPS; i++)
		test(path);

	return (0);
}

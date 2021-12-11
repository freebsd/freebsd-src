#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2020 Peter Holm <pho@FreeBSD.org>
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

# Attempt to reproduce
# "panic: pmap_release: pmap 0xfffffe012fb61b08 resident count 1 != 0"
# No problems seen.

# Test scenario suggestion by kib@

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1
[ `uname -p` != "amd64" ] && exit 0

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/mmap39.c
mycc -o mmap39 -Wall -Wextra -O0 -g mmap39.c -static || exit 1
rm -f mmap39.c
cd $odir

set -e
mount | grep "on $mntpoint " | grep -q /dev/md && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 2g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
set +e

for i in `jot 128`; do
	proccontrol -m aslr -s disable $dir/mmap39 &
	pids="$pids $!"
done
s=0
for pid in $pids; do
	wait $pid
	r=$?
	[ $s -ne 0 ] && s=$r
done

[ -f mmap39.core -a $s -eq 0 ] &&
    { ls -l mmap39.core; mv mmap39.core $dir; s=1; }
cd $odir

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
	[ $i -eq 6 ] &&
	    { echo FATAL; fstat -mf $mntpoint; exit 1; }
done
mdconfig -d -u $mdstart
rm -rf $dir/mmap39
exit $s

EOF
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <err.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

void
test(void)
{
	size_t i, len;
	void *p;

	p = (void *)0x8000000000; /* 512G */
	len = 0x200000;		  /*   2M */
	if ((p = mmap(p, len, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0)) ==
	    MAP_FAILED)
		err(1, "mmap");

	if (p != (void *)0x8000000000)
		errx(1, "mmap address fail");

	for (i = 0; i < len; i += PAGE_SIZE)
		*(char *)(p + i) = 1;

	if (munmap(p, len) == -1)
		err(1, "munmap()");

	_exit(0);
}

int
main(void)
{
	time_t start;
	pid_t pid;

	sleep(5);
	start = time(NULL);
	while (time(NULL) - start < 120) {
		if ((pid = fork()) == 0)
			test();
		if (waitpid(pid, NULL, 0) != pid)
			err(1, "waitpid");
	}

	return (0);
}

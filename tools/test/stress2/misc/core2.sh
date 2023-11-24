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

# Test multiple (parallel) core dumps and umount

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

odir=`pwd`

cd /tmp
sed '1,/^EOF/d' < $odir/$0 > core2.c
mycc -o core2 -Wall -Wextra -O0 core2.c || exit 1
rm -f core2.c
cd $RUNDIR

mount | grep "on $mntpoint " | grep -q md$mdstart && umount $mntpoint
[ -c /dev/mn$mdstart ] && mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

touch /tmp/continue
for i in `jot 64`; do
	mkdir -p $mntpoint/d$i
	(cd $mntpoint; /tmp/core2) &
done
rm -f /tmp/continue

for i in `jot 60`; do
	umount $mntpoint 2>/dev/null || sleep 1
	mount | grep -q "on $mntpoint " || break
done
wait
mount | grep -q "on $mntpoint " &&
    umount -f $mntpoint
mdconfig -d -u $mdstart
rm -f /tmp/core2
exit
EOF
#include <sys/mman.h>

#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define SIZ 1L * 128 * 1024 * 1024

void *p;

int
main(void)
{
	size_t len;

	len = SIZ;
	p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);

	while (access("/tmp/continue", R_OK) == 0)
		usleep(1);

	raise(SIGSEGV);

	return (0);
}

#!/bin/sh

#
# Copyright (c) 2014 EMC Corp.
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

# Test multiple (parallel) core dumps and umount -f

# "panic: vn_finished_write: neg cnt" seen.
# http://people.freebsd.org/~pho/core4.sh
# Fixed in r274501

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > core4.c
mycc -o core4 -Wall -Wextra -O0 -g core4.c || exit 1
rm -f core4.c

mount | grep -q "$mntpoint" && umount $mntpoint
mdconfig -l | grep -q $mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 2g -u $mdstart

newfs $newfs_flags md$mdstart > /dev/null
for i in `jot 20`; do
	mount /dev/md$mdstart $mntpoint
	chmod 777 $mntpoint
	su $testuser -c "(cd $mntpoint; /tmp/core4)" &
	su $testuser -c "(cd $mntpoint; /tmp/core4)" &
	su $testuser -c "(cd $mntpoint; /tmp/core4)" &
	sleep .5
	sleep .`jot -r 1 1 9`
	umount -f $mntpoint
	wait
done 2>/dev/null

while mount | grep -q "on $mntpoint "; do
	umount $mntpoint || sleep 1
done
mdconfig -d -u $mdstart
rm -f /tmp/core4
exit 0
EOF
#include <sys/mman.h>
#include <sys/param.h>

#include <err.h>
#include <signal.h>
#include <unistd.h>

#define SIZ (1024L * 1024 * 1024)

int
main(void)
{
	(void)mmap(NULL, SIZ, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);
	usleep(1000);
	raise(SIGSEGV);

	return (0);
}

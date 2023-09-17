#!/bin/sh

#
# Copyright (c) 2016 Dell EMC
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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# "panic: vnode_pager_generic_getpages: sector size 8192 too large" seen
# with an 8k sector size:
# https://people.freebsd.org/~pho/stress/log/gnop2.txt
# Fixed by r307626

. ../default.cfg

kldstat | grep -q geom_nop || { gnop load 2>/dev/null || exit 0 &&
    notloaded=1; }
dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/mmap5.sh > $dir/gnop2.c
mycc -o gnop2  -Wall -Wextra gnop2.c || exit 1
rm -f gnop2.c
cd $odir

test() {
	. ../default.cfg

	mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
	[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart

	set -e
	mdconfig -a -t swap -s 2g -u $mdstart
	gnop create -S $1 /dev/md$mdstart
	newfs $newfs_flags /dev/md$mdstart.nop > /dev/null
	mount /dev/md$mdstart.nop $mntpoint
	chmod 777 $mntpoint
	set +e

	dd if=/dev/zero of=$mntpoint/file bs=1k count=333 status=none
	/tmp/gnop2 $mntpoint/file

	while mount | grep $mntpoint | grep -q /dev/md; do
		umount $mntpoint || sleep 1
	done
	gnop destroy /dev/md$mdstart.nop
	mdconfig -d -u $mdstart
}

gnop status || exit 1

for i in 1k 2k 4k 8k; do
	test $i
done

[ $notloaded ] && gnop unload
rm -f /tmp/gnop2
exit 0

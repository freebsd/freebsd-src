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

# Test newfs with different sector size.
# newfs(8) issue fixed by r323157.
# mount(8) still fails with a sector size > 8k.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

kldstat | grep -q geom_nop || { gnop load 2>/dev/null || exit 0 &&
    loaded=1; }
gnop status > /dev/null || exit 1

s=0
set -e
# Fails with a sector size > 8k.
#for i in 1k 2k 4k 8k 16k 32k 64k; do
for i in 1k 2k 4k 8k; do
	echo $i
	mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
	[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart

	mdconfig -a -t swap -s 2g -u $mdstart
	gnop create -S $i /dev/md$mdstart
	newfs $newfs_flags /dev/md$mdstart.nop > /dev/null ||
	    { s=1; continue; }
	mount /dev/md$mdstart.nop $mntpoint ||
	    { gnop destroy /dev/md$mdstart.nop; mdconfig -d -u $mdstart
	    s=1; break; }
	chmod 777 $mntpoint

	(cd $mntpoint; jot 100 | xargs touch)

	while mount | grep $mntpoint | grep -q /dev/md; do
		umount $mntpoint || sleep 1
	done
	checkfs /dev/md$mdstart.nop || s=1
	gnop destroy /dev/md$mdstart.nop
	mdconfig -d -u $mdstart
done

[ $loaded ] && gnop unload
exit $s

#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2023 Peter Holm <pho@FreeBSD.org>
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

# Test mounting of snapshots for different UFS types

# Seen: mount of a -O1 snapshot failed
# Fixed by:
# f1549d7d5229 Write out corrected superblock when creating a UFS/FFS snapshot.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

set -u
s=0
m2=$((mdstart + 1))
mp2=$mntpoint$m2
[ -d $mp2 ] || mkdir -p $mp2
mount | grep -q "on $mntpoint " && umount -f $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart
[ -c /dev/md$m2 ] && mdconfig -d -u $m2
mdconfig -a -t swap -s 2g -u $mdstart || exit 1
for newfs_flags in "-O2" "-U" "-j" "-O1"; do
	echo "newfs $newfs_flags md$mdstart"
	newfs $newfs_flags md$mdstart > /dev/null
	mount /dev/md$mdstart $mntpoint
	touch $mntpoint/file

	rm -f $mntpoint/.snap/stress2
	mksnap_ffs $mntpoint $mntpoint/.snap/stress2 || { s=1; break; }
	mdconfig -a -t vnode -f $mntpoint/.snap/stress2 -u $m2 -o readonly ||
		{ s=2; break; }
	mount -t ufs -o ro /dev/md$m2 $mp2 || {
	    echo "mount of a $newfs_flags snapshot failed"
	    dumpfs -s /dev/md$m2; s=3; break; }
	[ -f $mp2/file ] || { s=4; ls -l $mp2; }
	umount $mp2
	mdconfig -d -u $m2
	umount $mntpoint
done
mount | grep -q "on $mntpoint " && umount $mntpoint

mdconfig -d -u $mdstart
exit $s

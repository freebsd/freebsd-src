#!/bin/sh

#
# Copyright (c) 2016 EMC Corp.
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

# vfs_write_suspend() test scenario.
# The unusual combination of newfs(8) flags: J and j triggers:
# fsync: giving up on dirty
# GEOM_JOURNAL: Cannot suspend file system /mnt (error=35). (EAGAIN)

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

[ $((`sysctl -n hw.usermem` / 1024 / 1024 / 1024)) -le 10 ] &&
    exit 0

md1=$mdstart
md2=$((mdstart + 1))

size=5g
jsize=3g

for u in $md1 $md2; do
	[ -c /dev/md$u ] && mdconfig -d -u $u
	mdconfig -a -t swap -s $size -u $u
done

gmirror load > /dev/null 2>&1 && unload=1
gmirror label -v -b split -s 2048 data /dev/md$md1 /dev/md$md2 \
    || exit 1
[ "`sysctl -in kern.geom.mirror.launch_mirror_before_timeout`" = "0" ] &&
    sleep $((`sysctl -n kern.geom.mirror.timeout` + 1))

gjournal load > /dev/null 2>&1
gjournal label -s $jsize /dev/mirror/data > /dev/null ||
    { gmirror stop data; exit 1; }
sleep .5
s=1
if [ -c /dev/mirror/data.journal ]; then
	newfs -J -j /dev/mirror/data.journal > /dev/null
	mount -o async /dev/mirror/data.journal $mntpoint || exit 1

	chmod 777 $mntpoint

	export runRUNTIME=10m
	export RUNDIR=$mntpoint/stressX

	su $testuser -c 'cd ..; ./run.sh disk.cfg'
	s=0

	gjournal sync
	umount $mntpoint
	while mount | grep $mntpoint | grep -q /mirror/data; do
		umount $mntpoint || sleep 1
	done
else
	echo "FAIL /dev/mirror/data.journal not found"
fi
gjournal stop /dev/mirror/data
gjournal unload
gmirror stop data
[ $unload ] && gmirror unload

for u in $md2 $md1; do
	mdconfig -d -u $u
done
exit $s

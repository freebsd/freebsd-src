#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2022 Peter Holm <pho@FreeBSD.org>
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

# Parallel mount and umount test.
# Copy of unionfs9.sh, with a subdirectory mount point.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

mounts=3	# Number of parallel scripts
CONT=/tmp/unionfs9.continue

set -e
mdconfig -a -t swap -s 256m -u $mdstart
newfs $newfs_flags -n md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint

for i in `jot $mounts $((mdstart + 2))`; do
	mdconfig -a -t swap -s 512m -u $((mdstart + i))
	newfs $newfs_flags -n md$((mdstart + i)) > /dev/null
	mkdir -p ${mntpoint}$i
	mount /dev/md$((mdstart + i)) ${mntpoint}$i
	mkdir -p ${mntpoint}$i/dir
done
set +e
echo Pre:
mount | grep mnt

(cd $mntpoint; jot 500 | xargs touch)
(cd ../testcases/swap; ./swap -t 5m -i 20 > /dev/null) &

# Start the parallel tests
touch $CONT
for i in `jot $mounts $((mdstart + 2))`; do
	while [ -f $CONT ]; do
		find ${mntpoint}$i -type f -maxdepth 2 -ls > \
		    /dev/null 2>&1
	done &
	# The test: Parallel mount and unmounts
	start=`date +%s`
	(
		while [ $((`date +%s` - start))  -lt 300 ]; do
			mount_unionfs $mntpoint ${mntpoint}$i/dir
			opt=$([ `jot -r 1 0 1` -eq 0 ] && echo "-f")
			while mount | grep -q ${mntpoint}$i/dir; do
				umount $opt ${mntpoint}$i/dir
			done
		done > /dev/null 2>&1
		rm -f $CONT
	) &
done
while [ -f $CONT ] ; do sleep 1; done
while pgrep -q swap; do pkill swap; done
wait
echo Post:
mount | grep mnt

for i in `jot $mounts $((mdstart + 2))`; do
	umount ${mntpoint}$i > /dev/null 2>&1
	mdconfig -d -u $((mdstart + i))
	rmdir ${mntpoint}$i
done
while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
	[ $((n += 1)) -gt 300 ] && { echo FAIL; exit 1; }
done
mdconfig -d -u $mdstart
exit 0

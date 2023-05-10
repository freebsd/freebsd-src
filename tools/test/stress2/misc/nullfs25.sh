#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018 Dell EMC Isilon
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

# Variation of nullfs17.sh WiP

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

mounts=4	# Number of parallel scripts
: ${nullfs_srcdir:=$mntpoint}
: ${nullfs_dstdir:=$mntpoint}
CONT=/tmp/nullfs25.continue

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -a -t swap -s 1g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint
(cd $mntpoint; jot 500 | xargs touch)
(cd ../testcases/swap; ./swap -t 5m -i 20 > /dev/null) &

for i in `jot $mounts $mdstart`; do
	[ ! -d ${nullfs_dstdir}$i ] && mkdir ${nullfs_dstdir}$i
	mount | grep -q " ${nullfs_dstdir}$i " &&
	    umount ${nullfs_dstdir}$i
done

# Start the parallel tests
touch $CONT
for i in `jot $mounts $mdstart`; do
	while [ -f $CONT ]; do
		find ${nullfs_dstdir}$i -type f -maxdepth 2 -ls > \
		    /dev/null 2>&1
	done &
	# The test: Parallel mount and unmounts
	start=`date +%s`
	(
		while [ $((`date +%s` - start))  -lt 300 ]; do
			mount_nullfs $nullfs_srcdir ${nullfs_dstdir}$i > \
			    /dev/null 2>&1
			opt=$([ `jot -r 1 0 1` -eq 0 ] && echo "-f")
			while mount | grep -q ${nullfs_dstdir}$i; do
				umount $opt ${nullfs_dstdir}$i > \
				    /dev/null 2>&1
			done
		done
		rm -f $CONT
	) &
done
while [ -f $CONT ] ; do sleep 1; done
while pgrep -q swap; do pkill swap; done
wait

for i in `jot $mounts`; do
	umount ${nullfs_dstdir}$i > /dev/null 2>&1
done
while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
	[ $((n += 1)) -gt 300 ] && { echo FAIL; exit 1; }
done
mdconfig -d -u $mdstart
exit 0

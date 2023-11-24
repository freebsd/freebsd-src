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

# Variation of crossmp3.sh

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

N=`sysctl -n hw.ncpu`
usermem=`sysctl -n hw.usermem`
[ `swapinfo | wc -l` -eq 1 ] && usermem=$((usermem/100*80))
size=$((usermem / 1024 / 1024 / N))

mounts=$N		# Number of parallel scripts

if [ $# -eq 0 ]; then
	for i in `jot $mounts`; do
		m=$(( i + mdstart - 1 ))
		[ ! -d ${mntpoint}$m ] && mkdir ${mntpoint}$m
		mount | grep "${mntpoint}$m " | grep -q md$m &&
		    umount ${mntpoint}$m
		mdconfig -l | grep -q md$m && mdconfig -d -u $m

		mdconfig -a -t swap -s ${size}m -u $m
		newfs $newfs_flags md${m} > /dev/null 2>&1
	done

	# start the parallel tests
	for i in `jot $mounts`; do
		m=$(( i + mdstart - 1 ))
		./$0 $m &
		./$0 find &
	done

	wait

	for i in `jot $mounts`; do
		m=$(( i + mdstart - 1 ))
		mdconfig -d -u $m
		rm -f $D$m
	done
	exit 0
else
	touch /tmp/crossmp.continue
	if [ $1 = find ]; then
		while [ -f /tmp/crossmp.continue ]; do
			find ${mntpoint}* -type f > /dev/null 2>&1
		done
	else
		# The test: Parallel mount and unmount
		m=$1
		for i in `jot 200`; do
			mount /dev/md${m} ${mntpoint}$m
			chmod 777 ${mntpoint}$m
			l=`jot -r 1 65535`
			dd if=/dev/zero of=$mntpoint/$i bs=$l count=100 \
		            status=none
			rm -f $mntpoint/$i

			while mount | grep -q "on ${mntpoint}$m "; do
				opt=$([ $((`date '+%s'` % 2)) -eq 0 ] &&
				    echo "-f")
				umount $opt ${mntpoint}$m > /dev/null 2>&1
			done
		done
		rm -f /tmp/crossmp.continue
	fi
fi

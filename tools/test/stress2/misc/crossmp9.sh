#!/bin/sh

#
# Copyright (c) 2016 Dell EMC Isilon
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

# Parallel mount and umount of file systems, while using getfsstat(2) via
# mount(8).

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

mounts=15		# Number of parallel scripts
cont=/tmp/crossmp.continue
mdstart=$mdstart	# Use md unit numbers from this point

if [ $# -eq 0 ]; then
	touch $cont
	for i in `jot $mounts`; do
		m=$(( i + mdstart - 1 ))
		[ ! -d ${mntpoint}$m ] && mkdir ${mntpoint}$m
		mount | grep "${mntpoint}$m " | grep -q md$m &&
		    umount ${mntpoint}$m
		mdconfig -l | grep -q md$m &&  mdconfig -d -u $m

		mdconfig -a -t swap -s 512m -u $m
		newfs $newfs_flags md${m} > /dev/null 2>&1
	done

	# start the parallel tests
	for i in `jot $mounts`; do
		m=$(( i + mdstart - 1 ))
		./$0 $m &
		./$0 find $m &
	done
	wait

	for i in `jot $mounts`; do
		m=$(( i + mdstart - 1 ))
		mdconfig -d -u $m
		rm -f $D$m
	done
	exit 0
else
	if [ $1 = find ]; then
		m=$2
		while [ -r $cont ]; do
			df ${mntpoint}$m
			mount
		done > /dev/null 2>&1
	else

		# The test: Parallel mount and unmounts
		start=`date '+%s'`
		while [ $((`date '+%s'` - start)) -lt 300 ] ; do
			m=$1
			mount /dev/md${m} ${mntpoint}$m
			while mount | grep -qw ${mntpoint}$m; do
				opt=$([ $((`date '+%s'` % 2)) -eq 0 ] &&
				    echo "-f")
				umount $opt ${mntpoint}$m > /dev/null 2>&1
			done
		done
		rm -f $cont
	fi
fi

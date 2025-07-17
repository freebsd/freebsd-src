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

# Parallel mount and umount of file systems

# "panic: userret: Returning with 1 locks held" seen:
# https://people.freebsd.org/~pho/stress/log/mark165.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

mounts=15		# Number of parallel scripts
cont=/tmp/msdos6.continue
mdstart=$mdstart	# Use md unit numbers from this point
part=a

if [ $# -eq 0 ]; then
	touch $cont
	mycc -o /tmp/fstool -Wall -Wextra -O2 ../tools/fstool.c || exit 1
	for i in `jot $mounts`; do
		m=$(( i + mdstart - 1 ))
		[ ! -d ${mntpoint}$m ] && mkdir ${mntpoint}$m
		mount | grep "$mntpoint" | grep -q md$m && umount ${mntpoint}$m
		mdconfig -l | grep -q md$m &&  mdconfig -d -u $m

		mdconfig -a -t swap -s 1g -u $m
		gpart create -s bsd md$m > /dev/null
		gpart add -t freebsd-ufs md$m > /dev/null
		newfs_msdos -F 32 -b 8192 /dev/md${m}$part > /dev/null 2>&1
		mount -t msdosfs /dev/md${m}$part ${mntpoint}$m
		(mkdir ${mntpoint}$m/test$i; cd ${mntpoint}$m/test$i; /tmp/fstool -l -f 100 -n 100 -s ${i}k)
		umount ${mntpoint}$m > /dev/null 2>&1
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
	done
	rm -f /tmp/fstool

else
	if [ $1 = find ]; then
		while [ -r $cont ]; do
			find ${mntpoint}* -type f > /dev/null 2>&1
		done
	else

		# The test: Parallel mount and unmounts
		start=`date '+%s'`
		while [ $((`date '+%s'` - start)) -lt 300 ]; do
			m=$1
			mount -t msdosfs /dev/md${m}$part ${mntpoint}$m
			while mount | grep -qw $mntpoint$m; do
				opt=$([ $((`date '+%s'` % 2)) -eq 0 ] && echo "-f")
				umount $opt ${mntpoint}$m > /dev/null 2>&1
			done
		done
		rm -f $cont
	fi
fi

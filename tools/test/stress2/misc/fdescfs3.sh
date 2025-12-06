#!/bin/sh

#
# Copyright (c) 2025 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

mounts=15		# Number of parallel scripts
mdstart=$mdstart	# Use md unit numbers from this point

if [ $# -eq 0 ]; then
	for i in `jot $mounts`; do
		m=$(( i + mdstart - 1 ))
		[ ! -d ${mntpoint}$m ] && mkdir ${mntpoint}$m
		mount | grep "$mntpoint" | grep -q md$m && umount ${mntpoint}$m
	done
	../testcases/swap/swap -t 2m -i 20 &

	# start the parallel tests
	touch /tmp/$0
	for i in `jot $mounts`; do
		m=$(( i + mdstart - 1 ))
		./$0 $m &
		./$0 find $m > /dev/null 2>&1 &
	done
	wait
else
	if [ $1 = find ]; then
		while [ -r /tmp/$0 ]; do
			ls -lR ${mntpoint}*
		done
	else

		# The test: Parallel mount and unmounts
		start=`date +%s`
		while [ $((`date +%s` - start)) -lt 120 ]; do
			m=$1
			mount -t fdescfs null ${mntpoint}$m
			while mount | grep -qw $mntpoint$m; do
				opt=$([ $((`date '+%s'` % 2)) -eq 0 ] && echo "-f")
				umount $opt ${mntpoint}$m > /dev/null 2>&1
			done
		done
		rm -f /tmp/$0
	fi
fi

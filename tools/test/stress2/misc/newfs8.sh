#!/bin/sh

#
# Copyright (c) 2025 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Variation of newfs.sh with VM pressure added

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

set -u
mount | grep "$mntpoint" | grep md$mdstart > /dev/null &&
    umount $mntpoint
mdconfig -l | grep md$mdstart > /dev/null &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 20g -u $mdstart

log=/tmp/newfs.sh.log
s=0
export RUNDIR=$mntpoint/stressX
export runRUNTIME=2m
export RUNTIME=$runRUNTIME
export CTRLDIR=$mntpoint/stressX.control
start=`date '+%s'`
for opt in -O2 -U -j; do
	blocksize=4096
	while [ $blocksize -le 65536 ]; do
		for i in 8 4 2 1; do
			fragsize=$((blocksize / i))
			echo "`date +%T` newfs $opt -b $blocksize -f $fragsize"\
			    "md$mdstart"
			newfs $opt -b $blocksize -f $fragsize \
			    md$mdstart > /dev/null || { s=1; continue; }
			[ "$opt" = "-O2" ] && tunefs -n disable md$mdstart > /dev/null 2>&1
			mount /dev/md$mdstart $mntpoint || { s=2; continue; }
			chmod 777 $mntpoint
			su $testuser -c \
				"(cd ..; ./run.sh io.cfg > /dev/null 2>&1)" &
			sleep 30
			while pkill swap; do :; done
			while pkill -U $testuser; do :; done
			../tools/killall.sh || { echo "Failed at $opt -b $blocksize -f $fragsize$"; \
				exit 3; }
			wait
			while mount | grep "$mntpoint" | \
			    grep -q md$mdstart; do
				umount $mntpoint > /dev/null 2>&1 || sleep 1
			done
			fsck -fy /dev/md$mdstart > $log 2>&1
			grep -q "WAS MODIFIED" $log && {
				s=4
				cat $log
			}
		done
		blocksize=$((blocksize * 2))
	done
	if [ $((`date '+%s'` - start)) -gt 3600 ]; then
		echo "Timed out in $opt -b $blocksize -f $fragsize$"
		s=5
		break
	fi
done
mdconfig -d -u $mdstart
rm -f $log
exit $s

#!/bin/sh

#
# Copyright (c) 2016 Dell EMC
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

# Quota version of parallelmount.sh
# "umount busy seen:
# https://people.freebsd.org/~pho/stress/log/parallelmount2.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ -z "$DEBUG" ] && exit 0 # Waiting for fix
[ "`sysctl -in kern.features.ufs_quota`" != "1" ] && exit 0

. ../default.cfg

parallel=40

if [ $# -eq 0 ]; then
	[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart
	mdconfig -a -t swap -s 10m -u $mdstart || exit 1
	newfs $newfs_flags md$mdstart > /dev/null
	export PATH_FSTAB=/var/tmp/fstab.$$
	echo "/dev/md$mdstart $mntpoint ufs rw,userquota 2 2" > $PATH_FSTAB
	mount /dev/md$mdstart $mntpoint
	set `df -ik $mntpoint | tail -1 | awk '{print $4,$7}'`
	export QK=$(($1 / 10 * 8))
	export QI=$(($2 / 10 * 8))
	edquota -u -f $mntpoint -e $mntpoint:$((QK - 50)):$QK:$((QI - 50 )):$QI $testuser
	quotaon $mntpoint
	umount $mntpoint

	# start the parallel tests
	for i in `jot $parallel`; do
		./$0 $i &
	done

	while kill -0 $! 2> /dev/null; do
		for i in `jot 100`; do
			find $mntpoint > /dev/null 2>&1
		done
	done
	wait
	rm -f $PATH_FSTAB

        for i in `jot 6`; do
		mount | grep -q "on $mntpoint " || break
                umount $mntpoint && break || sleep 10
        done
        [ $i -eq 6 ] && exit 1
	mdconfig -d -u $mdstart
	exit 0
else
	start=`date '+%s'`
	while [ $((`date '+%s'` - start)) -lt 600 ]; do
		mount /dev/md$mdstart $mntpoint
		quotaon $mntpoint
		umount $mntpoint
		mount
	done > /dev/null 2>&1
fi

#!/bin/sh

#
# Copyright (c) 2015 EMC Corp.
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

# Parallel mount and umount of zfs file systems.

# Page fault seen:
# https://people.freebsd.org/~pho/stress/log/avg002.txt
# Fixed by r309090.

# Page fault seen:
# https://people.freebsd.org/~pho/stress/log/crossmp7.txt
# Fixed by r352437.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ `uname -m` = "i386" ] && exit 0
[ `sysctl -n kern.kstack_pages` -lt 4 ] && exit 0

. ../default.cfg

mounts=15		# Number of parallel scripts

if [ $# -eq 0 ]; then
	kldstat -v | grep -q zfs.ko  || { kldload zfs.ko ||
	    exit 0; loaded=1; }
	zpool list | egrep -q "^stress2_tank" && zpool destroy stress2_tank

	u1=$mdstart
	u2=$((u1 + 1))
	u3=$((u2 + 1))

	[ -c /dev/md$u1 ] && mdconfig -d -u $u1
	[ -c /dev/md$u2 ] && mdconfig -d -u $u2
	[ -c /dev/md$u3 ] && mdconfig -d -u $u3

	mdconfig -s 512m -u $u1
	mdconfig -s 512m -u $u2
	mdconfig -s 512m -u $u3

	zpool create stress2_tank raidz md$u1 md$u2 md$u3

	for i in `jot $mounts`; do
		zfs create stress2_tank/test$i
		zfs umount stress2_tank/test$i
	done

	# start the parallel tests
	touch /tmp/crossmp7.continue
	for i in `jot $mounts`; do
		./$0 $i &
		./$0 find &
	done

	wait

	zpool destroy stress2_tank
	[ -n "$loaded" ] && kldunload zfs.ko
	mdconfig -d -u $u1
	mdconfig -d -u $u2
	mdconfig -d -u $u3
	exit 0
else
	if [ $1 = find ]; then
		while [ -f /tmp/crossmp7.continue ]; do
			find /stress2_tank -type f > /dev/null 2>&1
		done
	else
		# The test: Parallel mount and unmounts
		m=$1
		start=`date +%s`
		while [ $((`date +%s`- start)) -lt 300 ]; do
			zfs mount     stress2_tank/test$m
			zfs umount -f stress2_tank/test$m
		done 2>/dev/null
		rm -f /tmp/crossmp7.continue
	fi
fi

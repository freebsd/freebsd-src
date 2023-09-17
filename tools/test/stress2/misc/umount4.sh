#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Peter Holm
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

# A "(cd /mnt; umount /mnt)" test scenario, which triggered random memory
# corruptions.

# Page fault seen: https://people.freebsd.org/~pho/stress/log/kostik1230.txt
# First seen with r354204
# Initial fix by r354367

. ../default.cfg
[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

mount | grep -q "on $mntpoint " && umount $mntpoint
mdconfig -l | grep -q md$mdstart && mdconfig -d -u $mdstart

mdconfig -a -t swap -s 1g -u $mdstart || exit 1

for opt in "-U" "-j"; do
	echo "newfs $opt md$mdstart"
	newfs $opt md$mdstart > /dev/null
	mount /dev/md$mdstart $mntpoint || exit 1
	pids=""
	for i in `jot 3`; do
		while true; do
			dd if=/dev/zero of=$mntpoint/$i bs=1m \
			    count=`jot -r 1 1 10` status=none
		done &
		pids="$pids $!"
	done

	start=`date +%s`
	while [ $((`date +%s` - start)) -lt 60 ]; do
		(cd $mntpoint; umount $mntpoint)
	done > /dev/null 2>&1
	kill $pids
	wait

	while true; do
		umount $mntpoint && break
		sleep 1
	done
done
mdconfig -d -u $mdstart
exit 0

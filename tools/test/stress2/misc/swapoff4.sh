#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Peter Holm
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

# A variation of Mark's swapoff3.sh

# "panic: Last close with active requests" seen:
# https://people.freebsd.org/~pho/stress/log/mark134.txt
# Fixed by r358024-6

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1
dir=`dirname $diskimage`
[ `df -k $dir | tail -1 | awk '{print $4}'` -lt \
    $((20 * 1024 * 1024)) ] &&
    { echo "Need 20GB on $dir"; exit 0; }

set -e
md1=$mdstart
md2=$((md1 + 1))
[ `sysctl -n vm.swap_total` -gt 0 ] && { swapoff -a > /dev/null; off=1; }
truncate -s 10G $dir/swap1
mdconfig -a -t vnode -f $dir/swap1 -u $md1
swapon /dev/md$md1
truncate -s 10G $dir/swap2
mdconfig -a -t vnode -f $dir/swap2 -u $md2
swapon /dev/md$md2
set +e

export swapINCARNATIONS=25
(cd ../testcases/swap; ./swap -t 4m -i 20 -l 100) &
for i in `jot 90`; do
	used=`swapinfo | tail -1 | awk '{print $3}'`
	[ $used -gt 0 ] && break
	sleep 2
done

n=0
while pgrep -q swap; do
	for dev in /dev/md$md1 /dev/md$md2; do
		for i in `jot 3`; do
			swapoff $dev && break
		done
		swapon $dev
		n=$((n + 1))
	done
done
[ $n -lt 100 ] && { echo "Only performed $n swapoffs"; s=1; } || s=0
wait
for dev in /dev/md$md1 /dev/md$md2; do
	swapoff $dev
done
mdconfig -d -u $md1
mdconfig -d -u $md2
rm -f $dir/swap1 $dir/swap2
[ $off ] && swapon -a > /dev/null
exit 0

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

# tmpfs(5) version of fifo2.sh
# No problems seen on HEAD.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/fifo2.sh > fifo2.c
rm -f /tmp/fifo2
mycc -o fifo2 -Wall -Wextra -O2 -g fifo2.c -lpthread || exit 1
rm -f fifo2.c

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint

mount -o size=1g -t tmpfs tmpfs $mntpoint
chmod 777 $mntpoint
for i in `jot 5`; do
	mkfifo $mntpoint/f$i
	chmod 777 $mntpoint/f$i
done

daemon sh -c "(cd $odir/../testcases/swap; ./swap -t 10m -i 20 -l 100)" > \
    /dev/null
sleeptime=12
st=`date '+%s'`
while [ $((`date '+%s'` - st)) -lt $((10 * sleeptime)) ]; do
	(cd $mntpoint; /tmp/fifo2) &
	while ! pgrep -q fifo2; do :; done
	start=`date '+%s'`
	while [ $((`date '+%s'` - start)) -lt $sleeptime ]; do
		pgrep -q fifo2 || break
		sleep .5
	done
	while pgrep -q fifo2; do pkill -9 fifo2; done
	wait
done
pkill -9 swap fifo2
while pgrep -q "swap|fifo2"; do pkill -9 swap fifo2; done

for i in `jot 10`; do
	mount | grep -q "on $mntpoint "  && \
		umount $mntpoint > /dev/null 2>&1 && break
	sleep 10
done
s=0
mount | grep -q "on $mntpoint " &&
    { echo "umount $mntpoint failed"; s=1; }
rm -f /tmp/fifo2
exit $s

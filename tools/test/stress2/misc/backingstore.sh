#!/bin/sh

#
# Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
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

# Test sparse backing store

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

D=$diskimage
export here=`pwd`

m=$mdstart

mount | grep "$mntpoint" | grep -q md$m && umount $mntpoint$m
mdconfig -l | grep -q md$m &&  mdconfig -d -u $m

dd if=/dev/zero of=$D$m bs=100m count=1 status=none || exit 1

mdconfig -a -t vnode -f $D$m -u $m

newfs md${m} > /dev/null 2>&1
[ -d $mntpoint$m ] || mkdir -p $mntpoint$m
mount $opt /dev/md$m $mntpoint$m

n=$m
m=$((m + 1))

mount | grep "$mntpoint$m" | grep -q md$m && umount $mntpoint$m
mdconfig -l | grep -q md$m &&  mdconfig -d -u $m

truncate -s 500M $mntpoint$n/diskimage
mdconfig -a -t vnode -f $mntpoint$n/diskimage -u $m

newfs md${m} > /dev/null 2>&1
[ -d $mntpoint$m ] || mkdir -p $mntpoint$m
mount $opt /dev/md$m $mntpoint$m

export RUNDIR=$mntpoint$m/stressX
../testcases/rw/rw -t 5m -i 200 -h -n

while mount | grep -q $mntpoint$m; do
	flag=$([ $((`date '+%s'` % 2)) -eq 0 ] && echo "-f" || echo "")
	umount $flag $mntpoint$m > /dev/null 2>&1
done
mdconfig -l | grep -q md$m &&  mdconfig -d -u $m

m=$((m - 1))
while mount | grep -q $mntpoint$m; do
	umount $([ $((`date '+%s'` % 2)) -eq 0 ] && \
		echo "-f" || echo "") $mntpoint$m > /dev/null 2>&1
done
mdconfig -l | grep -q md$m &&  mdconfig -d -u $m
rm -f $D

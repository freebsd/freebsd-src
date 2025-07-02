#!/bin/sh

#
# Copyright (c) 2025 Peter Holm <pho@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Hunt for "vm_fault: pager read error, pid 32939 (mmap)"

# "panic: namei: unexpected flags: 0x10000000" seen:
# https://people.freebsd.org/~pho/stress/log/log0585.txt
# Fixed by: 58b2bd33aff7

. ../default.cfg

md=$mdstart
mp=$mntpoint
mdconfig -l | grep -q md$md && mdconfig -d -u $md
mount | grep -q "on $mp " && umount -f $mp

mdconfig -a -t swap -s 1g -u $md
newfs -U /dev/md$md > /dev/null
mount /dev/md$md $mp

export RUNDIR=$mp/stressX
../testcases/swap/swap -t 5m -i 20 -l 100 > /dev/null &
sleep 5
../testcases/mmap/mmap -t 5m -i 20 -l 100 > /dev/null 2>&1 &
sleep 5
umount -f $mp
pkill swap mmap
wait

mdconfig -d -u $md
exit 0

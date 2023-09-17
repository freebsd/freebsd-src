#!/bin/sh

#
# Copyright (c) 2014 EMC Corp.
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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# "panic: softdep_deallocate_dependencies: dangling deps" seen.
# "panic: softdep_write_inodeblock: indirect pointer #0 mismatch" seen.
# http://people.freebsd.org/~pho/stress/log/dangling.txt
# http://people.freebsd.org/~pho/stress/log/dangling2.txt
# https://people.freebsd.org/~pho/stress/log/kostik1101.txt

# Test scenario seems optimized for 4 CPUs.

. ../default.cfg

mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 4g -u $mdstart || exit 1
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint || exit 1
chmod 777 $mntpoint

export runRUNTIME=4m
export RUNDIR=$mntpoint/stressX
set `df -ik $mntpoint | tail -1 | awk '{print $4,$7}'`
export KBLOCKS=$(($1 / 4))
export INODES=$(($2 / 4))

export symlinkHOG=1
export rwHOG=1
export mkdirHOG=1

export LOAD=100
export symlinkLOAD=100
export rwLOAD=100
export mkdirLOAD=100
export TESTPROGS="
testcases/symlink/symlink
testcases/fts/fts
testcases/mkdir/mkdir
testcases/rw/rw
"

for i in `jot 10`; do
	su $testuser -c 'cd ..; ./testcases/run/run $TESTPROGS' > \
	    /dev/null 2>&1 &
	sleep 60
	kill $!
	../tools/killall.sh > /dev/null 2>&1
	../tools/killall.sh > /dev/null 2>&1
	wait
done

s=0
for i in `jot 6`; do
	umount $mntpoint && break || sleep 10
done
[ $i -eq 6 ] && s=1
mdconfig -d -u $mdstart
exit $s

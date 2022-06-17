#!/bin/sh

#
# Copyright (c) 2017 Dell EMC Isilon
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

# # ps -l
# UID   PID  PPID CPU PRI NI  VSZ  RSS MWCHAN STAT TT     TIME COMMAND
#   0 72280     1   0  20  0 5992 1768 mntref D     0- 0:03.20 umount /mnt

# https://people.freebsd.org/~pho/stress/log/kostik990.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ `swapinfo | wc -l` -eq 1 ] && exit 0

. ../default.cfg

mount | grep -q "on $mntpoint " && umount -f $mntpoint
[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart
mdconfig -a -t swap -s 5g -u $mdstart
newfs $newfs_flags md$mdstart > /dev/null
mount /dev/md$mdstart $mntpoint
chmod 777 $mntpoint

size=$((`sysctl -n hw.physmem` / 1024 / 1024))
[ $size -gt $((4 * 1024)) ] &&
    echo "RAM should be capped to 4GB for this test."

export RUNDIR=$mntpoint/stressX
export runRUNTIME=5m
export TESTPROGS="
testcases/symlink/symlink
testcases/openat/openat
testcases/fts/fts
testcases/lockf/lockf
"

export ftsINCARNATIONS=192
export lockfINCARNATIONS=132
export openatINCARNATIONS=218
export symlinkINCARNATIONS=201

export ftsLOAD=100
export lockfLOAD=100
export openatLOAD=100
export symlinkLOAD=100

export ftsHOG=1
export lockfHOG=1
export openatHOG=1
export symlinkHOG=1

su $testuser -c 'cd ..; ./testcases/run/run $TESTPROGS'

sync
umount $mntpoint &
sleep 5
kill -0 $! 2>/dev/null && { ps -lp$! | grep mntref && exit 1; }
wait

for i in `jot 6`; do
	mount | grep -q "on $mntpoint " || break
	umount $mntpoint && break || sleep 10
done
[ $i -eq 6 ] && exit 1
mdconfig -d -u $mdstart
exit 0

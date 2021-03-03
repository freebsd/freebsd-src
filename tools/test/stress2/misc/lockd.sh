#!/bin/sh

#
# Copyright (c) 2016 EMC Corp.
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

# Demonstrate rpc memory leak when remote lockd & statd are not running.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

pgrep -q lockd || { echo "lockd not running."; exit 0; }

[ -z "$nfs_export" ] && exit 0
ping -c 2 `echo $nfs_export | sed 's/:.*//'` > /dev/null 2>&1 ||
    exit 0
mount -t nfs -o tcp -o nfsv3 -o retrycnt=1 -o soft,timeout=1 \
    -o rw $nfs_export $mntpoint || exit 0

lockf -t 10 $mntpoint/$$.lock sleep 2 > /tmp/$$.log 2>&1
if grep -q "No locks available" /tmp/$$.log; then
	echo "Is lockd running on the remote host?"
	rm /tmp/$$.log
fi

rpc1=`vmstat -m | grep rpc | tail -1 | awk '{print $2}'`
start=`date '+%s'`
while [ $((`date '+%s'` - start)) -lt 60 ]; do
	for i in `jot 10`; do
		rm -f $mntpoint/$0.$$.$i
		lockf -t 10 $mntpoint/$0.$$.$i sleep 2 &
	done
	wait
done
rm -f $mntpoint/$0.$$*

s=0
for i in `jot 6`; do
	umount $mntpoint && break || sleep 10
done
[ $i -eq 6 ] && s=1
rpc2=`vmstat -m | grep rpc | tail -1 | awk '{print $2}'`
if [ $((rpc2 - rpc1)) -gt 2 ]; then
	echo "rpc memory leak. Was $rpc1, is $rpc2"
	s=2
fi
exit $s

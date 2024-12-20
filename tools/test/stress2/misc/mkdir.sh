#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2023 Peter Holm <pho@FreeBSD.org>
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

# Demonstrate incorrect "out of inodes" message with SU enabled.
# No issue seen with SU+J

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

set -eu
prog=$(basename "$0" .sh)
log=/tmp/$prog.log
s=0
mount | grep -q "on $mntpoint " && umount $mntpoint
mdconfig -l | grep -q md$mdstart && mdconfig -d -u $mdstart

mdconfig -a -t swap -s 100m -u $mdstart
[ $# -eq 1 ] && flags="$@" || flags="-Un"
echo "newfs $flags /dev/md$mdstart"
newfs $flags /dev/md$mdstart > /dev/null
[ "$flags" = "" ] && tunefs -n disable md$mdstart
mount /dev/md$mdstart $mntpoint
set +e

ifree1=`df -i $mntpoint | tail -1 | awk '{print $7}'`
before=`df -i $mntpoint`
n=$(((ifree1 - 5) / 10))
jot 10 | xargs -I% mkdir $mntpoint/%
start=`date +%s`
while [ $((`date +%s` - start)) -lt 180 ]; do
	for j in `jot 10`; do
		jot $n | xargs -P0 -I% mkdir $mntpoint/$j/%
		jot $n | xargs -P0 -I% rmdir $mntpoint/$j/%
	done
done 2>&1 | tee $log | head -5
[ -s $log ] && s=3
jot 10 | xargs -I% rmdir $mntpoint/%
umount $mntpoint; mount /dev/md$mdstart $mntpoint

ifree2=`df -i $mntpoint | tail -1 | awk '{print $7}'`
after=`df -i $mntpoint | tail -1`
if [ $ifree1 -ne $ifree2 ]; then
	echo "$before"
	echo "$after"
	s=1
	ls -alsrt $mntpoint | head -20
fi

umount $mntpoint
fsck -fy /dev/md$mdstart > $log 2>&1
grep -Eq "WAS MODIFIED" $log && { s=2; cat $log; }

mdconfig -d -u $mdstart
rm -f $log
exit $s

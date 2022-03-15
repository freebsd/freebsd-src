#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2022 Peter Holm
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

# Simple test to demonstrate EMLINK issue.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

md1=$mdstart
md2=$((md1 + 1))
mp1=/mnt$md1
mp2=/mnt$md2
mkdir -p $mp1 $mp2
set -e
for i in $mp1 $mp2; do
	mount | grep -q "on $i " && umount -f $i
done
for i in $md1 $md2; do
	mdconfig -l | grep -q md$i && mdconfig -d -u $i
done

mdconfig -a -t swap -s 2g -u $md1
mdconfig -a -t swap -s 2g -u $md2
newfs $newfs_flags -n md$md1 > /dev/null
newfs $newfs_flags -n md$md2 > /dev/null
mount /dev/md$md1 $mp1
mount /dev/md$md2 $mp2
df -i $mp1 $mp2
echo
mount -t unionfs -o noatime $mp1 $mp2
set +e

df -i $mp2
for i in `jot 100`; do
	jot 1000 | xargs -P0 -I% mkdir $mp2/a% || { s=1; break; }
	jot 1000 | xargs -P0 -I% rmdir $mp2/a%
done
[ $s ] && echo "Failed in loop #$i"
df -i $mp2 | tail -1

while mount | grep -Eq "on $mp2 .*unionfs"; do
	umount $mp2 && break
	sleep 5
done
umount $mp2
umount $mp1
mdconfig -d -u $md2
mdconfig -d -u $md1
exit $s

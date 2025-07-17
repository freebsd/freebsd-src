#!/bin/sh

#
# Copyright (c) 2022 Jason Harmening <jah@FreeBSD.org>
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

# Regression test:

# There is an issue, namely, when the lower file is already
# opened r/w, but its nullfs alias is executed. This situation obviously
# shall result in ETXTBUSY, but it currently does not.

# Based on nullfs10.sh by pho@, original test scenario by kib@

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

mdconfig -a -t swap -s 4g -u $md1
mdconfig -a -t swap -s 4g -u $md2
newfs $newfs_flags -n md$md1 > /dev/null
newfs $newfs_flags -n md$md2 > /dev/null
mount /dev/md$md1 $mp1
mount /dev/md$md2 $mp2

mount -t unionfs -o noatime $mp1 $mp2
set +e

mount | grep -E "$mp1|$mp2"

chmod 777 $mp1
chmod 777 $mp2

cp /bin/ls $mp1
chmod +w $mp1/ls
sleep 2 >> $mp1/ls &
sleep .5

# This line should cause a "$mp2/ls: Text file busy" error
$mp2/ls -l /bin/ls $mp1 $mp2 && echo FAIL || echo OK
kill $!
wait

while mount | grep -q "$mp2 "; do
	umount $mp2 || sleep 1
done

while mount | grep -q "$mp1 "; do
	umount $mp1 || sleep 1
done
mdconfig -d -u $md2
mdconfig -d -u $md1

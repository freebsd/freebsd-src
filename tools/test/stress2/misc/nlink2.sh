#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2022 Peter Holm <pho@FreeBSD.org>
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

# Simple test to demonstrate EMLINK issue on a SU file system.

# mkdir: /mnt10/a994: Too many links

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

echo "$newfs_flags" | grep -q U || exit 0
log=/tmp/nlink2.sh.log
md=$mdstart
mp=/mnt$md
mkdir -p $mp
set -e
mount | grep -q "on $mp " && umount -f $mp
mdconfig -l | grep -q md$md && mdconfig -d -u $md

mdconfig -a -t swap -s 1g -u $md
newfs $newfs_flags -n md$md > /dev/null
mount /dev/md$md $mp
set +e

start=`date +%s`
for i in `jot 1000`; do
	jot 1000 | xargs -P0 -I% mkdir $mp/a% || { s=1; break; }
	jot 1000 | xargs -P0 -I% rmdir $mp/a%
	t=$((`date +%s` - start))
	if [ $t -ge 300 ]; then
		echo "Warn: Timed out in loop #$i after $t seconds"
		break
	fi
done
if [ $s ]; then
	echo "Failed in loop #$i"
	df -i $mp
fi

umount $mp
fsck_ffs -fy /dev/md$md > $log 2>&1
grep -Eq "WAS MODIFIED" $log && { s=$((s + 2)); cat $log; }
mdconfig -d -u $md
rm -f $log
exit $s

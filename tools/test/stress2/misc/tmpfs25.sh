#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
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

# Regression test for:
# Bug 223015 - [tmpfs] [patch] tmpfs does not support sparse files

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
. ../default.cfg

set -eu
prog=$(basename "$0" .sh)
cont=/tmp/$prog.cont
sync=/tmp/$prog.sync
mount | grep -q "on $mntpoint " && umount $mntpoint
mount -t tmpfs -o size=100m dummy $mntpoint
iused=`df $mntpoint | tail -1 | awk '{print $3}'`

list="1m 10m 40m 45m 49m 50m 60m 100m 1g 4g"
r=0
for s in $list; do
	truncate -s $s $mntpoint/sparse || {
		echo "truncate -s $s failed"
		r=1; break 2
	}
	rm $mntpoint/sparse || break
	used=`df $mntpoint | tail -1 | awk '{print $3}'`
	[ $used -ne $iused ] && {
		echo "truncate -s $s test: $iused / $used"; r=1; break; }
done

touch $cont
for i in `jot 1000`; do
	[ $r -ne 0 ] && break
	file=$mntpoint/sparse.$i
	for s in $list; do
		[ ! -f $cont ] && break
		for n in `jot 300`; do [ -f $sync ] && break; sleep .2; done
		truncate -s $s $file || {
			echo "truncate -s $s failed"
			rm -f $cont
			break
		}
		[ -f $file ] || { echo "No file $file"; break; }
		rm $file || break
	done &
done
touch $sync
wait

used=`df $mntpoint | tail -1 | awk '{print $3}'`
if [ $used -ne $iused ]; then
	[ `ls -al $mntpoint | wc -l` -gt 3 ] &&
	    ls -al $mntpoint | head -10
	df -i $mntpoint
	fstat -f $mntpoint
fi
umount $mntpoint
rm -f $cont $sync

exit $r

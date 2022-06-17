#!/bin/sh

#
# Copyright (c) 2008-2011 Peter Holm <pho@FreeBSD.org>
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

# phk has seen freezes with this newfs option: "-b 32768 -f 4096 -O2"

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

size=$((32 * 1024 * 1024))

mount | grep "$mntpoint" | grep -q md$mdstart && umount $mntpoint
[ -c /dev/md$mdstart ] && mdconfig -d -u $mdstart

s=0
start=`date '+%s'`
while [ $size -le $((900 * 1024 * 1024)) ]; do
	mb=$((size / 1024 / 1024))
	rm -f $diskimage
	dd if=/dev/zero of=$diskimage bs=1m count=$mb status=none
	mdconfig -a -t vnode -f $diskimage -u $mdstart ||
	    { rm $diskimage; exit 1; }
	newfs -b 32768 -f 4096 -O2 md$mdstart > /dev/null 2>&1
	mount /dev/md$mdstart $mntpoint
	export RUNDIR=$mntpoint/stressX
	export runRUNTIME=30s
	export RUNTIME=$runRUNTIME
	export CTRLDIR=$mntpoint/stressX.control
	(cd ..; ./run.sh disk.cfg) > /dev/null
	while mount | grep "$mntpoint" | grep -q md$mdstart; do
		umount $mntpoint > /dev/null 2>&1
	done
	checkfs md$mdstart || s=1
	mdconfig -d -u $mdstart
	size=$((size + 32 * 1024 * 1024))
	if [ $((`date '+%s'` - start)) -gt 1200 ]; then
		echo "Timed out"
		s=1
		break
	fi
done
rm -f $diskimage
exit $s

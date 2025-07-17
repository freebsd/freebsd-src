#!/bin/sh

#
# Copyright (c) 2011 Peter Holm <pho@FreeBSD.org>
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

# Run with marcus.cfg on a malloc backed MD with option trim.
# Various panics seen with WiP kernel code. For example:
# panic: Bad tailq head 0xfffff80160cb7170 first->prev != head

. ../default.cfg

malloc_wait=`sysctl vm.md_malloc_wait | awk {'print $NF}'`
[ $malloc_wait != 1 ] && sysctl vm.md_malloc_wait=1
mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart

size="128m"
[ `uname -m` = "amd64" ] && size="1g"
[ $# -eq 0 ] && trim=-t
s=0
opt=""
[ "$newfs_flags" = "-U" ] && opt="-U -j"
export MAXSWAPPCT=80
for flag in ' ' $opt; do
	echo "mdconfig -a -t malloc -o reserve -s $size -u $mdstart"
	mdconfig -a -t malloc -o reserve -s $size -u $mdstart || exit 1

	echo "newfs $trim $flag md$mdstart"
	newfs $trim $flag md$mdstart > /dev/null
	mount /dev/md$mdstart $mntpoint || exit 1
	chmod 777 $mntpoint

	export runRUNTIME=5m
	export RUNDIR=$mntpoint/stressX

	su $testuser -c 'cd ..; ./run.sh marcus.cfg' > /dev/null 2>&1

	for i in `jot 6`; do
		umount $mntpoint && break || sleep 10
	done
	[ $i -eq 6 ] && { s=1; break; }
	checkfs /dev/md$mdstart || s=1
	mdconfig -d -u $mdstart
done
[ $malloc_wait != 1 ] && sysctl vm.md_malloc_wait=$malloc_wait
exit $s

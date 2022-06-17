#!/bin/sh

#
# Copyright (c) 2008, 2011 Peter Holm <pho@FreeBSD.org>
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

# Run a simple test on different FS variations, with and without disk full.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg
flag=/tmp/fs.sh.flag

ftest () {	# option, disk full
	local args="$@"
	[ $2 -eq 1 ] && df=", disk full" || df=""
	echo "`date '+%T'` newfs $1 md${mdstart}${part}$df"
	newfs $1 md$mdstart > /dev/null
	mount /dev/md$mdstart $mntpoint
	chmod 777 $mntpoint

	export RUNDIR=$mntpoint/stressX
	export runRUNTIME=1m
	disk=$(($2 + 1))	# 1 or 2
	set `df -ik $mntpoint | tail -1 | awk '{print $4,$7}'`
	export KBLOCKS=$(($1 * disk))
	export  INODES=$(($2 * disk))

	for i in `jot 2`; do
		rm -rf /tmp/stressX.control $RUNDIR
		su $testuser -c "(cd ..; ./run.sh disk.cfg)" > \
		    /dev/null 2>&1 &
		sleep 60
		../tools/killall.sh
		wait
	done

	for i in `jot 6`; do
		mount | grep -q "on $mntpoint " || break
		umount $mntpoint && break || sleep 10
		if [ $i -eq 6 ]; then
			touch $flag
			echo "Test \"$args\" FAIL"
			fstat -mf $mntpoint
			umount -f $mntpoint
		fi
	done
	checkfs /dev/md$mdstart || touch $flag
}

mount | grep "on $mntpoint " | grep -q md$mdstart && umount $mntpoint
[ -c /dev/md$mdstart ] &&  mdconfig -d -u $mdstart

mdconfig -a -t swap -s 20m -u $mdstart

ftest "-O 1"  0	# ufs1
ftest "-O 1"  1	# ufs1, disk full
ftest "-O 2"  0	# ufs2
ftest "-O 2"  1	# ufs2, disk full
ftest "-U"    0	# ufs2 + soft update
ftest "-U"    1	# ufs2 + soft update, disk full
ftest "-j"    0	# ufs2 + SU+J
ftest "-j"    1	# ufs2 + SU+J, disk full

mdconfig -d -u $mdstart
[ -f $flag ] && s=1 || s=0
rm -f $flag
exit $s

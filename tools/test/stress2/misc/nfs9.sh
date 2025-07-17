#!/bin/sh

#
# Copyright (c) 2008-2013 Peter Holm <pho@FreeBSD.org>
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

# Copy of nfs4.sh, where it was discovered that a missing killall.sh script
# turned up quite a few new panics. For example:

# vfs_mount_destroy: nonzero writeopcount
# Lock nfs not locked @ kern/vfs_default.c:462
# Assertion x == LK_SHARERS_LOCK(1) failed at kern/kern_lock.c:236

# "panic: SACK scoreboard must not be empty" seen:
# https://people.freebsd.org/~pho/stress/log/nfs9-2.txt

# Hang seen:
# $ ps -lp52442
# UID   PID  PPID CPU PRI NI   VSZ  RSS MWCHAN STAT TT     TIME COMMAND
#   0 52442 52120   0  20  0 11336 2868 nfsreq S     0  0:01.48 rm -rf /mnt/nfs9.dktfkpuf/stressX

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

[ -z "$nfs_export" ] && exit 0
ping -c 2 `echo $nfs_export | sed 's/:.*//'` > /dev/null 2>&1 ||
    exit 0

[ ! -d $mntpoint ] &&  mkdir $mntpoint
export RUNDIR=$mntpoint/nfs9.`jot -rc 8 a z | tr -d '\n'`/stressX
USE_TIMEOUT=1
s=0
start=`date +%s`
for i in `jot 10`; do
	mount | grep "on $mntpoint " | grep -q nfs && umount $mntpoint
	mount -t nfs -o tcp -o retrycnt=3 -o intr -o soft \
		-o rw $nfs_export $mntpoint
	sleep .5

	rm -rf $RUNDIR 2> /dev/null
	mkdir -p $RUNDIR || exit 1
	chmod -R 777 $RUNDIR 2> /dev/null
	export runRUNTIME=3m
	rm -rf /tmp/stressX.control/*

	if [ $i -eq 10 -o $((`date +%s` - start)) -ge 600 ]; then
		../tools/killall.sh
		su $testuser -c "rm -rf $RUNDIR" 2> /dev/null
		rm -rf `dirname $RUNDIR` || s=1
		done=1
	else
		su $testuser -c '(cd ..; ./run.sh all.cfg) > /dev/null \
		    2>&1' &
		sleep 60
	fi

	while mount | grep -q $mntpoint; do
		umount -f $mntpoint > /dev/null 2>&1
	done
	../tools/killall.sh || break
	kill -9 $! 2>/dev/null
	wait
	[ $done ] && break
done
exit $s

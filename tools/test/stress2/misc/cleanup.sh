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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

bname=`basename $mntpoint`
mounts=`mount | awk "/$bname/{print \\$3}"`
nmounts=`sysctl -n hw.ncpu`		# Max number of mounts in use
[ $nmounts -lt 31 ] && nmounts=31	# Arbitrary value
s=0
# Unmount the test mount points: /mnt, /mnt10 .. mnt31
for i in $mounts; do
	u=`echo $i | sed "s/\/$bname//"`
	[ -z "$u" ] && u=$mdstart
	echo "$u" | grep -Eq '^[0-9]+$' || continue
#	[ $u -lt $mdstart -o $u -gt $((mdstart + nmounts)) ] && continue
	while mount | grep -q "on $i "; do
		r=`fstat -mf $i 2>/dev/null | awk '$3 ~ /^[0-9]+$/ {print $3}'`
		if [ -n "$r" ]; then
			echo "cleanup.sh: kill $r"
			echo $r | xargs kill; sleep 1
		fi
		echo "cleanup.sh: umount -f $i"
		umount -f $i > /dev/null 2>&1 || s=1
		[ -z "$r" ] && break
	done
done

# Delete the test mount points /mnt10 .. /mnt31
for i in `ls -d $mntpoint* 2>/dev/null | grep -Ev '^$mntpoint$'`; do
	u=`echo $i | sed "s/\/$bname//"`
	echo "$u" | grep -Eq '^[0-9]+$' || continue
#	[ $u -lt $mdstart -o $u -gt $((mdstart + nmounts)) ] && continue
	if ! mount | grep -q "on $i "; then
		[ -d $i ] && find $i -delete \
			2>/dev/null
		rm -rf $i > /dev/null 2>&1
	fi
done

# Delete the memory disks
units=`mdconfig -l | sed 's/md//g'`
for u in $units; do
	if [ $u -ge $mdstart -a $u -lt $((mdstart + nmounts)) ]; then
		echo "cleanup.sh: mdconfig -d -u $u"
		mdconfig -d -u $u || s=1
		[ -c /dev/md$u ] && sleep .1
	fi
done

[ -d "$mntpoint" ] && (cd $mntpoint && find . -delete)
rm -f /tmp/.snap/stress2* /var/.snap/stress2*
rm -rf /tmp/stressX.control $RUNDIR
[ -d `dirname "$diskimage"` ] || mkdir -p `dirname "$diskimage"`
mkdir -p $RUNDIR
chmod 0777 $RUNDIR

# Delete $testuser's ipcs
ipcs | awk "\$5 ~/$testuser/ && \$6 ~/$testuser/ {print \"-\" \$1,\$2}" | \
    xargs -t ipcrm

# Modules
#mlist=/tmp/stress2.d/mlist
#find $mlist -mtime +1 -delete 2>/dev/null
#[ -f $mlist ] && touch $mlist ||
#    { kldstat | kldstat | awk '/\.ko$/ {print $NF}' > $mlist; \
#    echo "Updating $mlist"; }
#for m in `kldstat | awk '/\.ko$/ {print $NF}'`; do
#	grep -q $m $mlist && continue
#	echo "pty.ko mqueuefs.ko ioat.ko ums.ko" | grep -q $m && continue
#	echo "cleanup.sh: kldunload $m"
#	kldunload $m
#done
# unloading an active dtrace causes a panic
#kldstat | grep -q dtraceall.ko && kldunload dtraceall.ko
kldstat | grep -q ext2fs.ko && kldunload ext2fs.ko
[ $s -ne 0 ] && echo "cleanup.sh: FAIL $s"
exit $s

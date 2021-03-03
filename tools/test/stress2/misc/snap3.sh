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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

# Test with two snapshots
# 20070506 Page fault in g_io_request+0x7f

mount | grep -q "on /tmp (ufs," || exit 0
rm -f /tmp/.snap/stress2.1
rm -f /tmp/.snap/stress2.2
trap "rm -f /tmp/.snap/stress2.?" 0
mount | grep $mntpoint | grep -q md && umount $mntpoint
m1=$mdstart
m2=$((m1 + 1))
[ -c /dev/md$m1 ] &&  mdconfig -d -u $m1
[ -c /dev/md$m2 ] &&  mdconfig -d -u $m2

start=`date '+%s'`
while [ `date '+%s'` -lt $((start + 1200)) ]; do
	mksnap_ffs /tmp /tmp/.snap/stress2.1
	mksnap_ffs /tmp /tmp/.snap/stress2.2
	if [ -r /tmp/.snap/stress2.1 -a  -r /tmp/.snap/stress2.2 ]; then
		mdconfig -a -t vnode -f /tmp/.snap/stress2.1 -u $m1 -o readonly
		mdconfig -a -t vnode -f /tmp/.snap/stress2.2 -u $m2 -o readonly
		mount -o ro /dev/md$m1 $mntpoint
		sleep 3
		for i in `jot 3`; do
			umount $mntpoint && break
			sleep 2
		done
		mdconfig -d -u $m1
		mdconfig -d -u $m2
	fi
	rm -f /tmp/.snap/stress2.1 /tmp/.snap/stress2.2
done
exit 0

#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2021 Peter Holm <pho@FreeBSD.org>
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

# "userret: returning with the following locks held:" seen:
# https://people.freebsd.org/~pho/stress/log/log0188.txt

# Test scenario suggestion by: markj@

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
kldstat -v | grep -q zfs.ko  || { kldload zfs.ko; loaded=1; } ||
    exit 0

. ../default.cfg

here=`pwd`
mp1=/stress2_tank/test
u1=$mdstart
u2=$((u1 + 1))

set -e
mdconfig -l | grep -q md$u1 && mdconfig -d -u $u1
mdconfig -l | grep -q md$u2 && mdconfig -d -u $u2

mdconfig -s 2g -u $u1
mdconfig -s 2g -u $u2

zpool list | egrep -q "^stress2_tank" && zpool destroy stress2_tank
[ -d /stress2_tank ] && rm -rf /stress2_tank
zpool create stress2_tank md$u1 md$u2
zfs create stress2_tank/test
set +e

export RUNDIR=/stress2_tank/test/stressX
export runRUNTIME=5m
export LOAD=70
export TESTPROGS=`cd ..; find testcases/ -perm -1 -type f | \
    egrep -Ev "/run/|/tcp/|/udp/"`

(cd ..; ./testcases/run/run $TESTPROGS > /dev/null 2>&1) &

sleep 5
zfs snapshot stress2_tank/test@1

start=`date +%s`
while [ $((`date +%s` - start)) -lt 300 ]; do
	if [ `jot -r 1 1 2` -eq 1 ]; then
		zfs umount -f stress2_tank/test &&
		   zfs mount stress2_tank/test
	else
		zfs rollback stress2_tank/test@1
	fi
	sleep 5
	zfs list | grep -q /stress2_tank/test || break
done
wait

zfs umount stress2_tank/test
zfs destroy -r stress2_tank
zpool destroy stress2_tank

mdconfig -d -u $u1
mdconfig -d -u $u2
[ -n "$loaded" ] && kldunload zfs.ko
exit 0

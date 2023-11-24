#!/bin/sh

#
# Copyright (c) 2017 Dell EMC Isilon
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

# Watchdog fired:
# https://people.freebsd.org/~pho/stress/log/mark013.txt
# Fixed by: r327213

# watchdogd: https://people.freebsd.org/~pho/stress/log/kostik1245.txt

. ../default.cfg

outputfile=$RUNDIR/dd.outputfile
(cd ../testcases/swap; ./swap -t 30m -i 20) > /dev/null 2>&1 &
trap "rm -f ${outputfile}*" EXIT INT
N=2
NCPU=`sysctl -n hw.ncpu`
s=0
size=512
start=`date '+%s'`
while [ $((`date '+%s'` - start)) -lt 720 ]; do
	pids=""
	for i in `jot $N`; do
		dd if=/dev/zero of=${outputfile}$i bs=1m count=$size &
		pids="$pids $!"
	done > /dev/null 2>&1
	for pid in $pids; do
		wait $pid
		s=$?
		[ $s -ne 0 ] && break 2
	done
	N=$((N * 2))
	rm -f ${outputfile}*
	[ $N -gt $((NCPU * 2)) ] && break
done

while pgrep -q swap; do
	pkill -9 swap
done
exit $s

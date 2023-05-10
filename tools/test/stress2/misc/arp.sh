#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Dell EMC Isilon
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

# arp(8) seen waiting in "sbwait" (on non HEAD):
# UID   PID  PPID CPU PRI NI   VSZ  RSS MWCHAN STAT TT     TIME COMMAND
#   0 70090 68079   0  20  0  9872 2384 sbwait S+   u0  0:00.32 arp -da

[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1

start=`date +%s`
for i in `jot 3`; do
	while [ $((`date +%s` - start)) -lt 120 ]; do arp -da > /dev/null 2>&1; done &
	pids="$pids $!"
done
(cd ../testcases/swap; ./swap -t 2m -i 20 -h -l 100) > /dev/null
while [ $((`date +%s` - start)) -lt 120 ]; do sleep 1; done

for i in `jot 10`; do
	n=`pgrep -f arp.sh | wc -l`
	[ $n -eq 0 ] && break
	sleep 10
done
s=0
if [ $n -ne 0 ]; then
	ps -l | grep -v grep | grep arp
	pgrep arp | xargs procstat -k
	while pkill arp; do :; done
	s=1
fi
wait
exit $s

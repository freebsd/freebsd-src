#!/bin/sh

#
# Copyright (c) 2013 Peter Holm <pho@FreeBSD.org>
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

# "panic: kmem_malloc(4096): kmem_map too small ..." seen.
# "panic: sackhint bytes rtx >= 0" seen.
# "panic: pmap_release: pmap 0xfffffe012fb61b08 resident count 1 != 0" seen.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

[ `swapinfo | wc -l` -eq 1 ] && exit 0
[ $((`sysctl -n hw.usermem` / 1024 / 1024 / 1024)) -le 3 ] && exit 0

. ../default.cfg

rm -rf /tmp/stressX.control
[ ! -d $RUNDIR ] && mkdir -p $RUNDIR
chmod 777 $RUNDIR
export runRUNTIME=15m
export tcpLOAD=100
export swapLOAD=100
n=`su $testuser -c "limits | grep maxprocesses | awk '{print \\$NF}'"`
[ $n -gt 50000 ] && n=50000
n=$((n - `ps aux | wc -l`))
export tcpINCARNATIONS=$((n / 2 - 400))
[ $tcpINCARNATIONS -le 0 ] && exit 0
export TESTPROGS="
./testcases/tcp/tcp
./testcases/swap/swap
"

su $testuser -c '(cd ..; ./testcases/run/run $TESTPROGS)' &

sleep $((15 * 60))
while pkill -9 -U $testuser "run|tcp|swap"; do
	sleep .5
done
wait

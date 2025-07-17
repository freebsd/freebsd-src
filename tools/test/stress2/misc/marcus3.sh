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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# Watchdog fired:
# https://people.freebsd.org/~pho/stress/log/kostik853.txt
# but runs for a long time without watchdogd and deadlkres.

. ../default.cfg

dev=$(df -h `dirname $RUNDIR` | tail -1 | awk '{print $1}')
mount | grep $dev | grep -q journaled && exit 0
size=$((`sysctl -n hw.physmem` / 1024 / 1024))
[ $size -gt $((4 * 1024)) ] &&
    echo "RAM should be capped to 4GB for this test."
[ "`sysctl -n debug.deadlkres.sleepfreq 2>/dev/null`" = "3" ] &&
    { echo "deadlkres must be disabled for this test."; exit 0; }

n=`find ../testcases -perm -1 -type f | wc -l`
m=`su $testuser -c "limits | grep maxprocesses | awk '{print \\$NF}'"`
export runRUNTIME=15m
export INCARNATIONS=$((m / n))
export swapINCARNATIONS=$INCARNATIONS

timeout 16m su $testuser -c 'cd ..; ./run.sh marcus.cfg'
exit 0

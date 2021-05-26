#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2021 Peter Holm
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

# Trace random cc, c++ or make threads from a buildkernel
# Test scenario idea by kib@

old=`sysctl -in kern.kill_on_debugger_exit`
[ -z "$old" ] && exit 0
trap "sysctl kern.kill_on_debugger_exit=$old" EXIT INT

# Enable ptrace(2)d processes to continue
sysctl kern.kill_on_debugger_exit=0
./buildkernel.sh & kpid=$!
while true; do
	for i in `jot 8`; do
		rpid=`ps -l | grep -vwE 'grep|RE|defunct' | \
		    grep -E "(cc|c\+\+|make) " | \
		    sort -R | head -1 | awk '{print $2}'`
		[ -n "$rpid" ] && break
		sleep 1
	done
	[ -z "$rpid" ] && break
	kill -0 $rpid 2>/dev/null || continue
	ps -lp $rpid | tail -1
	echo "truss -f -o /dev/null -p $rpid"
	truss -f -o /dev/null -p $rpid & tpid=$!
	sleep `jot -r 1 5 10`
	pkill -9 truss
	wait $tpid
done
wait $kpid
exit $?

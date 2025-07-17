#!/bin/sh

#
# Copyright (c) 2015 EMC Corp.
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

# Report user mode leaks.

export COLUMNS=130
i=0
start=`date '+%s'`
/bin/echo -n "           "
ps -l | head -1
trap break SIGINT

while true; do
	i=$((i + 1))
	ps -axOvsz | sed 1d | awk -v loop=$i '$2 > 0 {print $1 "," $2 "," loop}'
	sleep 10
done | awk -F, '
{
	pid=$1
	size=$2
	loop=$3
	if (size > s[pid]) {
		n[pid]++
		if (n[pid] > 6) {
			print pid
			fflush
			n[pid] = 0
		}
		s[pid] = size
	}
	l[pid] = loop

	# Reap dead processes
	for (p in s) {
		if (l[p] < loop - 1) {
			delete s[p]
			delete n[p]
			delete l[p]
		}
	}
}' | while read p; do
	d=$(((`date '+%s'` - start) / 86400))
	r=`ps -lp$p | sed 1d`
	[ -n "$r" ] && echo "$d `date '+%T'` $r"
done
echo

#!/bin/sh

#
# Copyright (c) 2008-2011 Peter Holm
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

# Simple pmc test

# "panic: [pmc,4950] pm=0x2ddfe880 runcount 0" seen.
# https://people.freebsd.org/~pho/stress/log/pmc.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

kldstat -v | grep -q hwpmc  || { kldload hwpmc; loaded=1; }

event=`pmcstat -L 2>/dev/null | tr -d '\t' | sort -R | head -1`
if [ -n "$event" ]; then
	for i in `jot 2`; do
		pmcstat -P $event  -O /tmp/sample.out.$i find -x /var \
		    -name not.there &
	done
	sleep 1

	export runRUNTIME=5m
	pgrep -q pmcstat &&
	    (cd ..; ./run.sh vfs.cfg)
	wait
fi
[ $loaded ] && kldunload hwpmc
rm -f /tmp/sample.out.*
exit 0

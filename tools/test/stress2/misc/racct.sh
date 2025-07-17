#!/bin/sh

#
# Copyright (c) 2018 Dell EMC Isilon
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

# Regression test inspired by Andriy Gapon for Bug 222027 - panic on
# non-zero RACCT destroy.

# "panic: destroying non-empty racct ..." seen.
# https://people.freebsd.org/~pho/stress/log/racct.txt
# "Page fault in slab_free_item()" seen:
# https://people.freebsd.org/~pho/stress/log/racct-2.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ "`sysctl -in kern.racct.enable`" != "1" ] &&
    echo "Note: kern.racct.enable is disabled"
pgrep -Sq accounting || { service accounting onestart && started=1; }
(cd ../testcases/swap; ./swap -t 2m -i 5 -v -l 100) > /dev/null &
sleep .5
start=`date +%s`
while [ $((`date +%s` - start)) -lt 120 ]; do
	pids=""
	for i in `jot 5`; do
		for i in `jot 500`; do
			exec su -c xuser -m root -c ':' &
		done &
		pids="$pids $!"
	done
	wait $pids
	while [ `pgrep su | wc -l` -gt 100 ]; do sleep 1; done
done
wait
[ $started ] && service accounting onestop
exit 0

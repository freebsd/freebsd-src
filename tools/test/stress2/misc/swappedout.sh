#!/bin/sh

#
# Copyright (c) 2014 EMC Corp.
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

# Processes stay in "swapped out" state even after swapoff
# Broken by r254304.

# "panic: page 0xfffffe00070e7d30 is neither wired nor queued":
# https://people.freebsd.org/~pho/stress/log/swappedout-3.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

(cd ../testcases/swap; ./swap -t 2m -i 20 -v) > /dev/null
ps auxww | awk '{print $8}' | egrep -q ".W"  || exit 0
swapoff -a > /dev/null || exit 1
if ps auxww | awk '{print $8}' | egrep -q ".W"; then
	echo Fail
	echo "swapinfo:"
	swapinfo
	echo ""
	ps -l | head -1
	ps auxww | awk '{print $2, $8}' | egrep "[^ ]W" | while read l; do
		set $l
		ps -lp$1 | tail -1
	done
fi
swapon -a > /dev/null
exit 0

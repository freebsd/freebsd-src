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

# Test OOM killing.

# https://people.freebsd.org/~pho/stress/log/kostik847.txt
# Fixed by r290915 and r290917

# Expect:
# kernel: pid 5654 (sort), uid 0, was killed: out of swap space

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ `sysctl -n hw.physmem` -gt $(( 1 * 1024 * 1024 * 1024)) ] &&
    echo "RAM should be capped to 1GB for this test."
[ `sysctl -n hw.physmem` -gt $(( 8 * 1024 * 1024 * 1024)) ] && exit 0
[ `sysctl -n vm.swap_total` -gt 0 ] && { swapoff -a; off=1; }

for i in `jot 4`; do
	sort < /dev/zero &
done
wait
[ -n "$off" ] && swapon -a

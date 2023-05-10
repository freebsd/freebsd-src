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

# Test with out of swap space:
# swp_pager_getswapspace(32): failed

# "panic: freeing free block" sen with WiP kernel code:
# https://people.freebsd.org/~pho/stress/log/dougm027.txt

. ../default.cfg
[ `id -u` -ne 0 ] && echo "Must be root!" && exit 1
[ `sysctl -n hw.physmem` -lt $(( 5 * 1024 * 1024 * 1024)) ] && exit 0
cc -o /tmp/swap5 -Wall -Wextra -O2 ../tools/swap.c || exit 1
swapoff -a
mdconfig -a -t malloc -o reserve -s 4g -u $mdstart || exit 1
swapon /dev/md$mdstart || exit 1
for i in `jot 3`; do
	/tmp/swap5 -t 180 -p 85
done
swapoff /dev/md$mdstart
mdconfig -d -u $mdstart
swapon -a
rm -f /tmp/swap5
exit 0

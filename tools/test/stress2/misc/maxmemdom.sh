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

# Demonstrate that "options MAXMEMDOM" is broken. (NUMA test)
# panic: vm_page_alloc: missing page
# https://people.freebsd.org/~pho/stress/log/maxmemdom.txt
# Fixed in r293640.

. ../default.cfg

[ `sysctl -n vm.ndomains` -eq 1 ] && exit 0
size=$((`sysctl -n hw.physmem` / 1024 / 1024))
need=$((size * 2))
d1=$diskimage.1
d2=$diskimage.2
rm -f $d1 $d2
[ `df -k $(dirname $diskimage) | tail -1 | awk '{print int($4 / 1024)}'` -lt \
    $need ] && printf "Need %d MB on %s.\n" $need `dirname $diskimage` && exit
timeout 30m sh -ce "
        dd if=/dev/zero of=$d1 bs=1m count=$size status=none
        cp $d1 $d2
"
s=$?
[ $s -eq 124 ] && echo "Timed out"

rm -f $d1 $d2
exit $s

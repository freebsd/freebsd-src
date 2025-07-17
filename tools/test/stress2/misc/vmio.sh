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

# Out of swap seen with no swap disk:
# dd 'if=/dev/zero' 'of=/var/tmp/diskimage.1' 'bs=1m' 'count=3427'
# pid 598 (ntpd), uid 0, was killed: out of swap space
# pid 656 (sendmail), uid 0, was killed: out of swap space
# pid 659 (sendmail), uid 25, was killed: out of swap space
# https://people.freebsd.org/~pho/stress/log/alan020.txt

# Deadlock seen:
# https://people.freebsd.org/~pho/stress/log/kostik845.txt
# Fixed by r290920.

. ../default.cfg

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ `sysctl -n hw.physmem` -gt $(( 8 * 1024 * 1024 * 1024)) ] && exit 0
[ `swapinfo | wc -l` -eq 1 ] || { swapoff -a; off=1; }
size=$((`sysctl -n hw.physmem` / 1024 / 1024))
need=$((size * 2))
d1=$diskimage.1
d2=$diskimage.2
rm -f $d1 $d2 || exit 1
[ `df -k $(dirname $diskimage) | tail -1 | awk '{print int($4 / 1024)}'` \
    -lt $need ] &&
    printf "Need %d MB on %s.\n" $need `dirname $diskimage` && exit 0
trap "rm -f $d1 $d2" EXIT INT
dd if=/dev/zero of=$d1 bs=1m count=$size status=none
cp $d1 $d2
[ $off ] && swapon -a

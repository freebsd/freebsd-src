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

# Test scenario with 256MB RAM on a single CPU i386.

# "panic: ffs_checkblk: bad block -1" seen:
# https://people.freebsd.org/~pho/stress/log/256m.txt
# Fixed by r291743.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

[ `uname -m` = "i386" ] || exit 0
[ `sysctl -n hw.ncpu` -eq 1 ] || { echo "Single CPU test."; exit 0; }
[ `sysctl -n hw.physmem` -gt $((256 * 1024 * 1024)) ] &&
    { echo "RAM must be clamped to 256MB for this test."; exit 0; }
[ -f /usr/src/sys/i386/conf/GENERIC ] || exit 0

cd /usr/src
make -j 2 buildkernel KERNCONF=GENERIC
rm -rf /usr/obj/usr/src/sys/GENERIC

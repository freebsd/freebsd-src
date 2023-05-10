#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2020 Peter Holm
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

# Run the tools/uma/smrstress test by Jeffrey Roberson <jeff@FreeBSD.org>

# "panic: vm_page_assert_xbusied: page 0xf... not exclusive busy" seen
# https://people.freebsd.org/~pho/stress/log/jeff146.txt
# Fixed by r357392

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
dir=/usr/src/tools/uma/smrstress
mod=/boot/modules/smrstress.ko
[ -d $dir ] || exit 0

rm -f $mod
here=`pwd`
cd $dir
make all install
[ -f $mod ] || exit 1

(cd $here/../testcases/swap; ./swap -t 2m -i 20 -l 100) &
sleep 5
kldload smrstress.ko
kldunload smrstress.ko
pkill swap
wait

make clean
rm -f kldload.core $mod
exit 0

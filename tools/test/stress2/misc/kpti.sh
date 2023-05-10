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

# Simple test of kpti (Kernel page-table isolation) enable / disable

# Page fault seen: https://people.freebsd.org/~pho/stress/log/dougm061.txt
# Fixed by r354132

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
proccontrol -m kpti -q 2> /dev/null || exit 0
[ `sysctl -n vm.pmap.pti` -eq 1 ] && echo "Page Table Isolation enabled" ||
    echo "Page Table Isolation disabled"

prog=date
[ $# -eq 1 ] && prog=$1
start=`date +%s`
while [ $((`date +%s` - start)) -lt 120 ]; do
        for i in `jot 50`; do
                proccontrol -m kpti -s disable $prog &
                proccontrol -m kpti -s enable  $prog &
        done > /dev/null
	wait
done
exit 0

#!/bin/sh

#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018 Dell EMC Isilon
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

# Demonstrate disk queue completely full of TRIM commands.
# "14 seconds elapsed" seen on r337569.
# Fixed by rxxxxxx.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
trim=$(df -k | sed 1d | sort -rn +3 | awk '{print $NF}' | while read mp; do
        mount | grep "on $mp " | grep -q ufs || continue
        on=`mount | grep "on $mp " | awk '{print $1}'`
        dumpfs $on | head -20 | grep -q trim  || continue
        echo $on && break
done)
[ -z "$trim" ] && exit 0
mp=`mount | grep "$trim " | awk '{print $3}'`
echo "Found TRIM on $trim mounted as $mp"
free=`df -k $mp | tail -1 | awk '{print $4}'`
free=$((free / 1024 / 1024)) # GB
[ $free -lt 2 ] && exit 0
expect=-1
before=`mount -v | grep "$mp "`
for i in `jot 40`; do
        t=`date +%s`
        dd if=/dev/zero of=$mp/trim.data.$i bs=1m count=1k status=none
        rm $mp/trim.data.$i
        t=$((`date +%s` - t))
        [ $expect -eq -1 ] && expect=$t
        [ $t -le $((4 * expect)) ] && continue
        after=`mount -v | grep "$mp "`
        echo "$t seconds elapsed"
        echo "Before: $before"
        echo "After:  $after"
        exit 1
done
exit 0
